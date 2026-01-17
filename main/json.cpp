// json.c
//
#include "stdio.h"
#include <string>
#include <list>
#include "esp_log.h"

#include "lvgl.h"
#include "gt911.h"
#include "ArduinoJson.h"
#include "mqtt_client.h"

extern "C" char hardLvglLock(int timeout_ms);
extern "C" void hardLvglUnlock();

extern "C" uint16_t nvsRead(char *name);
extern "C" void nvsWrite(char *name, uint16_t value);

extern "C" void snapshot_event_cb(lv_event_t *e);

const char *TAG = "JSON";

const char *displayFile = "/spiffs/display.json";
JsonDocument jsonDisplay;

const char *mqttFile = "/spiffs/mqtt.json";
JsonDocument jsonMqtt;

LV_IMG_DECLARE(img_lvgl_logo);

LV_FONT_DECLARE(my_font_montserrat_18);
LV_FONT_DECLARE(my_font_montserrat_24);
LV_FONT_DECLARE(my_font_montserrat_30);

#define MAX_TOPIC_SIZE 64
#define MAX_TEXT_SIZE 32
#define MAX_DATA_SIZE 32
#define MAX_TOPICS 100

static uint16_t nbrTopics = 0;

struct objets_t
{
    char topic[MAX_TOPIC_SIZE];
};

objets_t objets[MAX_TOPICS];

// Objets lvgl
//
static lv_obj_t *tv = NULL;
static lv_style_t image_style;

extern "C" esp_mqtt_client_handle_t client;

static SemaphoreHandle_t datas_mux = NULL;
static std::list<std::string> datas;
static std::list<std::string> topics;

static lv_palette_t current_palette = LV_PALETTE_BLUE;

static lv_style_t style;
static lv_obj_t *ledMqtt;

static void event_button(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int32_t idx = (int32_t)lv_event_get_user_data(e);
        if ((idx > -1) && (idx < nbrTopics))
        {
            esp_mqtt_client_publish(client, objets[idx].topic, "0", 0, 0, 0);
        }
    }
}

static void event_slider(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED)
    {
        lv_obj_t *slider = lv_event_get_target_obj(e);
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", (int)lv_slider_get_value(slider));
        int32_t idx = (int32_t)lv_event_get_user_data(e);
        if ((idx > -1) && (idx < nbrTopics))
        {
            esp_mqtt_client_publish(client, objets[idx].topic, buf, 0, 0, 0);
        }
    }
}

static void event_arc(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED)
    {
        lv_obj_t *arc = lv_event_get_target_obj(e);
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", (int)lv_arc_get_value(arc));
        int32_t idx = (int32_t)lv_event_get_user_data(e);
        if ((idx > -1) && (idx < nbrTopics))
        {
            esp_mqtt_client_publish(client, objets[idx].topic, buf, 0, 0, 0);
        }
    }
}

static void event_checkbox(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        lv_obj_t *checkbox = lv_event_get_target_obj(e);
        uint8_t state = lv_obj_get_state(checkbox) & LV_STATE_CHECKED ? 1 : 0;
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", state);
        int32_t idx = (int32_t)lv_event_get_user_data(e);
        if ((idx > -1) && (idx < nbrTopics))
        {
            esp_mqtt_client_publish(client, objets[idx].topic, buf, 0, 0, 0);
         }
    }
}

static void event_arc_change(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        lv_obj_t *arc = lv_event_get_target_obj(e);
        lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);

        if (lv_event_get_param(e) == NULL)
        {
            lv_label_set_text_fmt(label, "%d", (int)lv_arc_get_value(arc));
        }
        else
        {
            lv_label_set_text(label, (const char *)lv_event_get_param(e));
        }
    }
}

static void event_chart(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *chart = lv_event_get_target_obj(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        lv_obj_invalidate(chart);
    }
    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE)
    {
        lv_coord_t *s = (lv_coord_t *)lv_event_get_param(e);
        *s = LV_MAX(*s, 20);
    }
    else if (code == LV_EVENT_DRAW_POST_END)
    {
        uint32_t id = lv_chart_get_pressed_point(chart);
        if (id == LV_CHART_POINT_NONE)
            return;

        LV_LOG_USER("Selected point %d", (int)id);

        lv_chart_series_t *ser = lv_chart_get_series_next(chart, NULL);
        int32_t value = 0;
        while (ser)
        {
            lv_point_t p;
            lv_chart_get_point_pos_by_id(chart, ser, id, &p);

            int32_t *y_array = lv_chart_get_series_y_array(chart, ser);
            if (y_array[id] != LV_CHART_POINT_NONE && y_array[id] >= 0)
            {

                /*Accumulate the values to show the rectangles at the top of each segment*/
                value += y_array[id];

                /*Draw a rectangle above the clicked point*/
                lv_layer_t *layer = lv_event_get_layer(e);
                lv_draw_rect_dsc_t draw_rect_dsc;
                lv_draw_rect_dsc_init(&draw_rect_dsc);
                draw_rect_dsc.bg_color = lv_color_black();
                draw_rect_dsc.bg_opa = LV_OPA_50;
                draw_rect_dsc.radius = 3;

                lv_area_t chart_obj_coords;
                lv_obj_get_coords(chart, &chart_obj_coords);
                lv_area_t rect_area;
                rect_area.x1 = chart_obj_coords.x1 + p.x - 25;
                rect_area.x2 = chart_obj_coords.x1 + p.x + 25;
                rect_area.y1 = chart_obj_coords.y1 + p.y - 10;
                rect_area.y2 = chart_obj_coords.y1 + p.y + 10;
                lv_draw_rect(layer, &draw_rect_dsc, &rect_area);

                /*Draw the value as label to the center of the rectangle*/
                char buf[16];
                lv_snprintf(buf, sizeof(buf), "%.1f", value / 10.0f);

                lv_draw_label_dsc_t draw_label_dsc;
                lv_draw_label_dsc_init(&draw_label_dsc);
                draw_label_dsc.color = lv_color_white();
                draw_label_dsc.text = buf;
                draw_label_dsc.text_local = 1;
                draw_label_dsc.align = LV_TEXT_ALIGN_CENTER;
                lv_area_t label_area = rect_area;
                lv_area_set_height(&label_area, lv_font_get_line_height(draw_label_dsc.font));
                lv_area_align(&rect_area, &label_area, LV_ALIGN_CENTER, 0, 0);
                lv_draw_label(layer, &draw_label_dsc, &label_area);
            }

            ser = lv_chart_get_series_next(chart, ser);
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        lv_obj_invalidate(chart);
    }
}

static void set_style_text_color(lv_obj_t *obj, std::string color)
{
    if (color == "orange")
        lv_obj_set_style_text_color(obj, lv_palette_main(LV_PALETTE_ORANGE), 0);
    else if (color == "blue")
        lv_obj_set_style_text_color(obj, lv_palette_main(LV_PALETTE_BLUE), 0);
    else if (color == "red")
        lv_obj_set_style_text_color(obj, lv_palette_main(LV_PALETTE_RED), 0);
    else if (color == "green")
        lv_obj_set_style_text_color(obj, lv_palette_main(LV_PALETTE_GREEN), 0);
    else if (color == "yellow")
        lv_obj_set_style_text_color(obj, lv_palette_main(LV_PALETTE_YELLOW), 0);
}

static lv_align_t get_align_from_string(std::string align)
{
    if (align == "center")
        return LV_ALIGN_CENTER;
    else if (align == "top_left")
        return LV_ALIGN_TOP_LEFT;
    else if (align == "top_mid")
        return LV_ALIGN_TOP_MID;
    else if (align == "top_right")
        return LV_ALIGN_TOP_RIGHT;
    else if (align == "bottom_left")
        return LV_ALIGN_BOTTOM_LEFT;
    else if (align == "bottom_mid")
        return LV_ALIGN_BOTTOM_MID;
    else if (align == "bottom_right")
        return LV_ALIGN_BOTTOM_RIGHT;
    else if (align == "left_mid")
        return LV_ALIGN_LEFT_MID;
    else if (align == "right_mid")
        return LV_ALIGN_RIGHT_MID;
    else
        return LV_ALIGN_TOP_LEFT;
}

static void extract_position_and_size(ArduinoJson::V742HB22::JsonVariant object, int16_t &x, int16_t &y, int16_t &w, int16_t &h, int16_t defW, int16_t defH)
{
    if (object.containsKey("x"))
        x = object["x"];
    else
        x = 0;

    if (object.containsKey("y"))
        y = object["y"];
    else
        y = 0;

    if (object.containsKey("w"))
        w = object["w"];
    else
        w = defW;

    if (object.containsKey("h"))
        h = object["h"];
    else
        h = defH;
}

static void set_align_from_string(ArduinoJson::V742HB22::JsonVariant object, lv_obj_t *obj, int16_t x, int16_t y)
{
    if (object.containsKey("align"))
    {
        std::string align = object["align"];
        lv_obj_align(obj, get_align_from_string(align), x, y);
    }
    else
    {
        lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x, y);
    }
}

static void createObjects(JsonArray &objects, lv_obj_t *parent)
{
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;
    int16_t min = 0;
    int16_t max = 0;

    for (auto object : objects)
    {
        std::string type = object["type"];

        lv_obj_t *child = NULL;

        if (type == "checkbox")
        {
            child = lv_checkbox_create(parent);

            extract_position_and_size(object, x, y, w, h, 100, 50);
            set_align_from_string(object, child, x, y);

            lv_obj_set_size(child, w, h);

            std::string texte = "Unknown";
            if (object.containsKey("text"))
            {
                std::string t = object["text"];
                texte = t;
            }
            lv_checkbox_set_text(child, texte.c_str());
        }
        else if (type == "arc")
        {
            child = lv_arc_create(parent);

            extract_position_and_size(object, x, y, w, h, 150, 150);
            set_align_from_string(object, child, x, y);

            lv_obj_set_size(child, w, h);
            lv_arc_set_rotation(child, 135);
            lv_arc_set_bg_angles(child, 0, 270);
            lv_arc_set_value(child, 0);

            if (object.containsKey("size"))
            {
                std::string size = object["size"];
                if (size == "large")
                    lv_obj_set_style_text_font(child, &my_font_montserrat_30, 0);
                else if (size == "medium")
                    lv_obj_set_style_text_font(child, &my_font_montserrat_24, 0);
                else
                    lv_obj_set_style_text_font(child, &my_font_montserrat_18, 0);
            }

            if ((object.containsKey("min")) && (object.containsKey("max")))
            {
                min = object["min"];
                max = object["max"];
                lv_arc_set_range(child, min, max);
            }

            lv_obj_t *label = lv_label_create(child);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

            lv_obj_add_event_cb(child, event_arc_change, LV_EVENT_VALUE_CHANGED, label);
            lv_obj_send_event(child, LV_EVENT_VALUE_CHANGED, NULL);
        }
        else if (type == "button")
        {
            child = lv_btn_create(parent);

            extract_position_and_size(object, x, y, w, h, 64, 64);
            set_align_from_string(object, child, x, y);

            lv_obj_set_size(child, w, h);
        }
        else if (type == "chart")
        {
            child = lv_chart_create(parent);

            extract_position_and_size(object, x, y, w, h, 64, 64);
            set_align_from_string(object, child, x, y);

            lv_chart_set_type(child, LV_CHART_TYPE_BAR);

            lv_chart_series_t *serie = lv_chart_add_series(child, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
            object["lv_chart_series_t"] = (uint32_t)serie;

            if (object.containsKey("points_count"))
            {
                int16_t pointsCount = object["points_count"];
                lv_chart_set_point_count(child, pointsCount);
            }

            if ((object.containsKey("min")) && (object.containsKey("max")))
            {
                min = object["min"];
                max = object["max"];
                lv_chart_set_range(child, LV_CHART_AXIS_PRIMARY_Y, min * 10, max * 10);
            }

            lv_obj_set_size(child, w, h);
            lv_obj_add_event_cb(child, event_chart, LV_EVENT_ALL, NULL);
        }
        else if ((type == "image") && (object.containsKey("name")))
        {
            child = lv_img_create(parent);

            extract_position_and_size(object, x, y, w, h, 48, 48);
            set_align_from_string(object, child, x, y);

            std::string name = object["name"];
            lv_image_set_src(child, name.c_str());
        }
        else if (type == "label")
        {
            child = lv_label_create(parent);

            extract_position_and_size(object, x, y, w, h, 48, 48);
            set_align_from_string(object, child, x, y);

            std::string texte = "Unknown";
            if (object.containsKey("text"))
            {
                std::string t = object["text"];
                texte = t;
            }

            lv_label_set_text(child, texte.c_str());

            if (object.containsKey("size"))
            {
                std::string size = object["size"];
                if (size == "large")
                    lv_obj_set_style_text_font(child, &my_font_montserrat_30, 0);
                else if (size == "medium")
                    lv_obj_set_style_text_font(child, &my_font_montserrat_24, 0);
                else
                    lv_obj_set_style_text_font(child, &my_font_montserrat_18, 0);
            }
            if (object.containsKey("color"))
            {
                std::string color = object["color"];
                set_style_text_color(child, color);
            }
        }
        else if (type == "panel")
        {
            child = lv_obj_create(parent);

            extract_position_and_size(object, x, y, w, h, 100, 100);
            set_align_from_string(object, child, x, y);

            lv_obj_add_style(child, &style, 0);
            lv_obj_set_size(child, w, h);
        }
        else if (type == "slider")
        {
            child = lv_slider_create(parent);

            extract_position_and_size(object, x, y, w, h, 150, 40);
            set_align_from_string(object, child, x, y);

            lv_obj_set_size(child, w, h);
        }
        else if (type == "tab")
        {
            if (object.containsKey("text"))
            {
                std::string str = object["text"];
                child = lv_tabview_add_tab(parent, str.c_str());
            }
        }

        if ((object.containsKey("topic")) && (child != NULL))
        {
            std::string topic = object["topic"];
            if ((topic.length() < MAX_TOPIC_SIZE) && ((nbrTopics + 1) < MAX_TOPICS))
            {
                if ((type == "arc"))
                {
                    strcpy(objets[nbrTopics].topic, topic.c_str());
                    nbrTopics++;
                    lv_obj_add_event_cb(child, event_arc, LV_EVENT_RELEASED, (void *)(nbrTopics - 1));
                }
                else if ((type == "button"))
                {
                    strcpy(objets[nbrTopics].topic, topic.c_str());
                    nbrTopics++;
                    lv_obj_add_event_cb(child, event_button, LV_EVENT_CLICKED, (void *)(nbrTopics - 1));
                }
                else if (type == "slider")
                {
                    strcpy(objets[nbrTopics].topic, topic.c_str());
                    nbrTopics++;
                    lv_obj_add_event_cb(child, event_slider, LV_EVENT_RELEASED, (void *)(nbrTopics - 1));
                }
                else if ((type == "checkbox"))
                {
                    strcpy(objets[nbrTopics].topic, topic.c_str());
                    nbrTopics++;
                    lv_obj_add_event_cb(child, event_checkbox, LV_EVENT_VALUE_CHANGED, (void *)(nbrTopics - 1));
                }
            }
        }
        if (child != NULL)
        {
            object["lv_obj_t"] = (uint32_t)child;
        }

        if (object.containsKey("objects") && (child != NULL))
        {
            JsonArray childs = object["objects"];
            createObjects(childs, child);
        }
    }
}

static void updateObjects(JsonArray &objects, char *topic, char *data)
{

    for (auto object : objects)
    {
        std::string type = object["type"];

        if ((object.containsKey("topic")) && (object.containsKey("lv_obj_t")))
        {
            std::string top = object["topic"];
            if (!strcmp(top.c_str(), topic))
            {
                lv_obj_t *obj = (lv_obj_t *)((uint32_t)object["lv_obj_t"]);
                if ((type == "label") && (object.containsKey("text")))
                {
                    std::string text = object["text"];
                    char texte[MAX_TEXT_SIZE + MAX_DATA_SIZE];
                    sprintf(texte, text.c_str(), data);

                    lv_label_set_text(obj, texte);
                }
                else if ((type == "image") && (object.containsKey("name")))
                {
                    std::string name = object["name"];
                    char nom[MAX_TEXT_SIZE + MAX_DATA_SIZE];
                    sprintf(nom, name.c_str(), data);

                    lv_img_set_src(obj, nom);
                }
                else if (type == "arc")
                {
                    char *ptr = data;
                    if (object.containsKey("text"))
                    {
                        std::string text = object["text"];
                        char texte[MAX_TEXT_SIZE + MAX_DATA_SIZE];
                        sprintf(texte, text.c_str(), data);
                        ptr = texte;
                    }
                    int16_t val = atoi(data);
                    lv_arc_set_value(obj, val);
                    lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, ptr);
                }
                else if (type == "chart")
                {
                    lv_chart_series_t *serie = (lv_chart_series_t *)((uint32_t)object["lv_chart_series_t"]);

                    char *ptr = data;

                    lv_chart_set_next_value(obj, serie, atof(ptr) * 10);
                    int idx = 0;
                    while (*ptr != 0)
                    {
                        ptr++;
                        if (*ptr == ',')
                        {
                            idx++;
                            ptr++;
                            lv_chart_set_next_value(obj, serie, atof(ptr) * 10);
                        }
                    }

                    lv_chart_refresh(obj);
                }
            }
        }

        if ((object.containsKey("topicCondition")) && (object.containsKey("conditions")) && (object.containsKey("lv_obj_t")))
        {
            std::string topicCondition = object["topicCondition"];
            if (!strcmp(topicCondition.c_str(), topic))
            {
                JsonArray conditions = object["conditions"];
                lv_obj_t *obj = (lv_obj_t *)((uint32_t)object["lv_obj_t"]);
                bool bColor = true;
                bool bName = true;
                bool bCondition = false;
                for (auto dataconditions : conditions)
                {
                    if (dataconditions.containsKey("value"))
                    {
                        std::string str = dataconditions["value"];
                        if (!strcmp(str.c_str(), data))
                        {
                            bCondition = true;
                        }
                    }
                    else if (dataconditions.containsKey("greater"))
                    {
                        int16_t val = atoi(data);
                        int16_t cond = dataconditions["greater"];
                        if (val > cond)
                        {
                            bCondition = true;
                        }
                    }
                    else if (dataconditions.containsKey("less"))
                    {
                        int16_t val = atoi(data);
                        int16_t cond = dataconditions["less"];
                        if (val < cond)
                        {
                            bCondition = true;
                        }
                    }
                    if (bCondition)
                    {
                        if (dataconditions.containsKey("color"))
                        {
                            std::string color = dataconditions["color"];
                            set_style_text_color(obj, color);
                            bColor = false;
                        }
                        if (dataconditions.containsKey("visible"))
                        {
                            std::string visible = dataconditions["visible"];
                            if (visible == "true")
                                lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            else
                                lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        if ((type == "image") && (dataconditions.containsKey("name")))
                        {
                            std::string name = dataconditions["name"];
                            lv_img_set_src(obj, name.c_str());
                            bName = false;
                        }
                        if ((type == "checkbox") && (dataconditions.containsKey("checked")))
                        {
                            std::string checked = dataconditions["checked"];
                            if (checked == "true")
                                lv_obj_add_state(obj, LV_STATE_CHECKED);
                            else
                                lv_obj_remove_state(obj, LV_STATE_CHECKED);
                        }
                        break;
                    }
                }
                if ((bColor) && (object.containsKey("color")))
                {
                    std::string color = object["color"];
                    set_style_text_color(obj, color);
                }
                if ((bName) && (object.containsKey("name")))
                {
                    std::string name = object["name"];
                    lv_img_set_src(obj, name.c_str());
                }
            }
        }

        if (object.containsKey("objects"))
        {
            JsonArray childs = object["objects"];
            updateObjects(childs, topic, data);
        }
    }
}

static void color_changer_anim_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_coord_t max_w = lv_obj_get_width(lv_obj_get_parent(obj)) - LV_DPX(20);
    lv_coord_t w;

    w = lv_map(v, 0, 256, LV_DPX(60), max_w);
    lv_obj_set_width(obj, w);
    lv_obj_align(obj, LV_ALIGN_BOTTOM_RIGHT, -LV_DPX(10), -LV_DPX(10));

    if (v > LV_OPA_COVER)
        v = LV_OPA_COVER;

    uint32_t i;
    for (i = 0; i < lv_obj_get_child_cnt(obj); i++)
    {
        lv_obj_set_style_opa(lv_obj_get_child(obj, i), v, 0);
    }
}

static void color_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);

    if (code == LV_EVENT_FOCUSED)
    {
        lv_obj_t *color_cont = lv_obj_get_parent(obj);
        if (lv_obj_get_width(color_cont) < LV_HOR_RES / 2)
        {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, color_cont);
            lv_anim_set_exec_cb(&a, color_changer_anim_cb);
            lv_anim_set_values(&a, 0, 256);
            lv_anim_set_time(&a, 200);
            lv_anim_start(&a);
        }
    }
    else if (code == LV_EVENT_CLICKED)
    {
        lv_palette_t *palette_primary = (lv_palette_t *)lv_event_get_user_data(e);
        lv_palette_t palette_secondary = (lv_palette_t)((*palette_primary) + 3); /*Use another palette as secondary*/
        if (palette_secondary >= LV_PALETTE_LAST)
            palette_secondary = (lv_palette_t)0;
#if LV_USE_THEME_DEFAULT
        lv_theme_default_init(NULL, lv_palette_main(*palette_primary), lv_palette_main(palette_secondary),
                              LV_THEME_DEFAULT_DARK, &my_font_montserrat_18);
#endif
        // lv_color_t color = lv_palette_main(*palette_primary);
        //  lv_style_set_text_color(&style_icon, color);
        current_palette = *palette_primary;
        nvsWrite("palette", current_palette);
    }
}

static void color_changer_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        lv_obj_t *color_cont = (lv_obj_t *)lv_event_get_user_data(e);
        if (lv_obj_get_width(color_cont) < LV_HOR_RES / 2)
        {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, color_cont);
            lv_anim_set_exec_cb(&a, color_changer_anim_cb);
            lv_anim_set_values(&a, 0, 256);
            lv_anim_set_time(&a, 200);
            lv_anim_start(&a);
        }
        else
        {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, color_cont);
            lv_anim_set_exec_cb(&a, color_changer_anim_cb);
            lv_anim_set_values(&a, 256, 0);
            lv_anim_set_time(&a, 200);
            lv_anim_start(&a);
        }
    }
}

static void color_changer_create(lv_obj_t *parent)
{
    static lv_palette_t palette[] = {
        LV_PALETTE_BLUE, LV_PALETTE_GREEN, LV_PALETTE_BLUE_GREY, LV_PALETTE_ORANGE,
        LV_PALETTE_RED, LV_PALETTE_PURPLE, LV_PALETTE_TEAL, LV_PALETTE_LAST};

    lv_obj_t *color_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(color_cont);
    lv_obj_set_flex_flow(color_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(color_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(color_cont, LV_OBJ_FLAG_FLOATING);

    lv_obj_set_style_bg_color(color_cont, lv_color_white(), 0);
    lv_obj_set_style_pad_right(color_cont, LV_DPX(55), 0);
    lv_obj_set_style_bg_opa(color_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(color_cont, LV_RADIUS_CIRCLE, 0);

    lv_obj_set_size(color_cont, LV_DPX(60), LV_DPX(60));

    lv_obj_align(color_cont, LV_ALIGN_BOTTOM_RIGHT, -LV_DPX(10), -LV_DPX(10));

    uint32_t i;
    for (i = 0; palette[i] != LV_PALETTE_LAST; i++)
    {
        lv_obj_t *c = lv_btn_create(color_cont);
        lv_obj_set_style_bg_color(c, lv_palette_main(palette[i]), 0);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_set_size(c, 30, 30);
        lv_obj_add_event_cb(c, color_event_cb, LV_EVENT_ALL, &palette[i]);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    }

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_add_flag(btn, (lv_obj_flag_t)(LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_STATE_CHECKED);
    lv_obj_set_style_pad_all(btn, 10, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(btn, color_changer_event_cb, LV_EVENT_ALL, color_cont);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_text_font(btn, &my_font_montserrat_18, 0);
    lv_obj_set_style_bg_img_src(btn, LV_SYMBOL_TINT, 0);

    lv_obj_set_size(btn, LV_DPX(50), LV_DPX(50));
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -LV_DPX(15), -LV_DPX(15));
}

static void snapshot_create(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_add_flag(btn, (lv_obj_flag_t)(LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_STATE_CHECKED);
    lv_obj_set_style_pad_all(btn, 10, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, snapshot_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_text_font(btn, &my_font_montserrat_30, 0);
    lv_obj_set_style_bg_img_src(btn, LV_SYMBOL_IMAGE, 0);

    lv_obj_set_size(btn, 56, 56);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 2, 2);
}

static void mqtt_create(lv_obj_t *parent)
{
    ledMqtt = lv_led_create(lv_screen_active());

    lv_obj_set_size(ledMqtt, 20, 20);
    lv_obj_align(ledMqtt, LV_ALIGN_TOP_RIGHT, -10, 20);
    lv_led_set_brightness(ledMqtt, 150);
    lv_led_set_color(ledMqtt, lv_palette_main(LV_PALETTE_GREEN));
    lv_led_on(ledMqtt);
}

extern "C" void jsonMqttConnected()
{
    lv_led_set_color(ledMqtt, lv_palette_main(LV_PALETTE_GREEN));
}

extern "C" void jsonMqttDisconnected()
{
    lv_led_set_color(ledMqtt, lv_palette_main(LV_PALETTE_RED));
}

extern "C" void jsonInit()
{
    datas_mux = xSemaphoreCreateRecursiveMutex();
    assert(datas_mux);

    FILE *f = fopen(displayFile, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Erreur File Display");
        return;
    }

    fseek(f, 0, SEEK_END);
    uint16_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = NULL;
    buf = (char *)heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM);
    assert(buf);

    fread(buf, size, 1, f);

    fclose(f);

    DeserializationError error = deserializeJson(jsonDisplay, buf);
    if (error)
    {
        ESP_LOGE(TAG, "Erreur Json");
        return;
    }

    current_palette = (lv_palette_t)nvsRead("palette");

    if (hardLvglLock(-1))
    {
        lv_style_init(&image_style);
        lv_style_set_img_recolor_opa(&image_style, LV_OPA_COVER);
        lv_style_set_img_recolor(&image_style, lv_palette_lighten(LV_PALETTE_GREY, 4));

        lv_style_init(&style);
        lv_style_set_radius(&style, 5);
        lv_style_set_pad_all(&style, 5);

        tv = lv_tabview_create(lv_screen_active());
        lv_obj_set_style_text_font(tv, &my_font_montserrat_18, 0);
        lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
        lv_tabview_set_tab_bar_size(tv, 60);

        lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tv);
        lv_obj_set_style_pad_left(tab_bar, 61, 0);
        lv_obj_set_style_pad_right(tab_bar, 40, 0);

        if (jsonDisplay.containsKey("objects"))
        {
            JsonArray objects = jsonDisplay["objects"];
            createObjects(objects, tv);
        }

        color_changer_create(tv);

        snapshot_create(tv);
        mqtt_create(tv);

        lv_palette_t palette_secondary = (lv_palette_t)(current_palette + 3);
        if (palette_secondary >= LV_PALETTE_LAST)
            palette_secondary = (lv_palette_t)0;
#if LV_USE_THEME_DEFAULT
        lv_theme_default_init(NULL, lv_palette_main(current_palette), lv_palette_main(palette_secondary),
                              LV_THEME_DEFAULT_DARK, &my_font_montserrat_18);
#endif

        hardLvglUnlock();
    }
}

extern "C" void jsonSubscribe()
{

    if (jsonDisplay.containsKey("mqtt"))
    {
        JsonArray topics = jsonDisplay["mqtt"];

        for (auto topic : topics)
        {
            if (topic.containsKey("topic"))
            {
                std::string name = topic["topic"];
                if (name.length() < MAX_TOPIC_SIZE)
                {
                    esp_mqtt_client_subscribe_single(client, name.c_str(), 1);
                }
            }
        }
    }
}

extern "C" void jsonDataEvent(esp_mqtt_event_handle_t event)
{
    std::string t;
    std::string d;

    if (xSemaphoreTakeRecursive(datas_mux, portMAX_DELAY) == pdTRUE)
    {
        for (int i = 0; i < event->topic_len; i++)
        {
            t.push_back(event->topic[i]);
        }
        for (int i = 0; i < event->data_len; i++)
        {
            d.push_back(event->data[i]);
        }

        topics.push_back(t);
        datas.push_back(d);

        xSemaphoreGiveRecursive(datas_mux);
    }
}

extern "C" void jsonUpdate()
{

    if (xSemaphoreTakeRecursive(datas_mux, portMAX_DELAY) == pdTRUE)
    {
        if (datas.empty())
        {
            xSemaphoreGiveRecursive(datas_mux);
            return;
        }

        std::string t = topics.front();
        topics.pop_front();
        std::string d = datas.front();
        datas.pop_front();

        xSemaphoreGiveRecursive(datas_mux);

        if (jsonDisplay.containsKey("objects"))
        {
            if (hardLvglLock(-1))
            {
                JsonArray objects = jsonDisplay["objects"];
                updateObjects(objects, (char *)t.c_str(), (char *)d.c_str());
                hardLvglUnlock();
            }
        }
    }
}

extern "C" void jsonParametersMqtt(esp_mqtt_client_config_t *mqtt_cfg)
{

    FILE *f = fopen(mqttFile, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Erreur File Mqtt");
        return;
    }

    fseek(f, 0, SEEK_END);
    uint16_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = NULL;
    buf = (char *)heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM);
    assert(buf);

    fread(buf, size, 1, f);

    fclose(f);

    DeserializationError error = deserializeJson(jsonMqtt, buf);
    if (error)
    {
        ESP_LOGE(TAG, "Erreur File Mqtt");
        return;
    }

    if (jsonMqtt.containsKey("mqtt"))
    {
        JsonObject parameters = jsonMqtt["mqtt"];
        if (parameters.containsKey("broker"))
        {
            std::string broker = parameters["broker"];
            mqtt_cfg->broker.address.uri = strdup(broker.c_str());
        }
        if (parameters.containsKey("username"))
        {
            std::string username = parameters["username"];
             mqtt_cfg->credentials.username = strdup(username.c_str());
        }
        if (parameters.containsKey("password"))
        {
            std::string password = parameters["password"];
            mqtt_cfg->credentials.authentication.password = strdup(password.c_str());
        }
    }
    else
    {
        ESP_LOGE(TAG, "Erreur Json mqtt");
    }
}
