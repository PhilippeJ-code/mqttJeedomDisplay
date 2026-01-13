idf_component_register(SRCS "json.cpp" "main.c" "../fonts/my_font_montserrat_18.c" "../fonts/my_font_montserrat_24.c" "../fonts/my_font_montserrat_30.c"
        EMBED_FILES "html_header.html" "html_footer.html" "folder.svg" "file.svg" "delete.svg"
        INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)                      