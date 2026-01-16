# MQTT Jeedom Display

### Description

MQTT Jeedom Display est un programme qui permet d'afficher des données en provenance de Jeedom et d'envoyer des commandes vers Jeedom.

Le programme s'exécute sur la platine ESP32-S3-LCD-4.3 de Waveshare et utilise lvgl pour l'affichage.

Les informations transitent via MQTT.

Si la connexion avec le Wifi échoue ( lors de la première connexion par exemple ), le programme va lancer sa propre connexion Wifi ce qui permettra de sélectionner votre réseau Wifi.

# Contenu
- [MQTT Jeedom Display](#mqtt-jeedom-display)
    - [Description](#description)
- [Contenu](#contenu)
- [Première fois](#première-fois)
- [Affichage](#affichage)
- [Topics](#topics)
- [Objets](#objets)
    - [Type tab](#type-tab)
    - [Type panel](#type-panel)
    - [Type label](#type-label)
    - [Type image](#type-image)
    - [Type arc](#type-arc)
    - [Type chart](#type-chart)
- [Composants tiers](#composants-tiers)
- [License](#license)

# Première fois

Si la connexion avec le réseau Wifi échoue ou lors de la première utilisation du programme, celui-ci lance son propre réseau Wifi sous le nom *mqtt_jeedom*, connectez vous sur ce réseau avec votre smartphone ( par exemple ). Un mot de passe sera demandé, introduisez *password*.

Vous pouvez maintenant accéder au gestionnaire Wifi à l'adresse *10.10.0.1*, si tout se passe bien vous pourrez choisir votre réseau, suivez ensuite les instructions pour vous connecter.

![choix_reseau](/images_readme/choix_reseau.jpg "choix_reseau")

Il faut maintenant créer un fichier *mqtt.json* sur base du fichier *mqtt_exemple.json*.

Après l'acquisition du réseau le client mqtt va essayer de se connecter sur base des paramètres contenu dans le fichier *mqtt.json*, deux boutons apparaissent dont un libellé "Wifi", cliquez sur celui-ci et relancez ensuite la platine.

Elle va redémarrer sur un écran noir mais une application Web sera disponible pour vous permettre de gèrer les fichiers contenus dans la platine. Repérez l'adresse qui a été attribuée par exemple *192.168.1.52* et surfez sur l'adresse *http://192.168.1.52/files*

![files](/images_readme/files.jpg "files")

Cliquez sur le fichier *mqtt_exemple.json* pour le télécharger, renommez le *mqtt.json* et introduisez les paramètres de votre broker. Téléchargez le fichier *mqtt.json* ainsi modifié et téléversez le sur la platine.

mqtt_exemple.json

```
{
    "mqtt" : {
        "broker": "mqtt://192.168.1.xxx",
        "username": "username",
        "password": "password"
    }
}
```
L'étape suivante consistera à créer un fichier display.json qui permettra de configure les topics utilisés et la façon de les afficher. Le fichier *display_exemple.json* pourra servir de base pour la configuration.

# Affichage

Configuration du fichier display.json
```
{
  "mqtt": 
  [
    {
      "topic": "topic/temperature"
    },
    {
      "topic": "topic/humidite"
    }
  ],
  "objects": 
  [
    {
      "type": "tab",
      "text": "Salon",
      "objects": 
      [
        {
          "type": "label",
          "align": "top_left",
          "x": 30,
          "y": 35,
          "text": "%s°C",
          "topic": "topic/temperature"
        },
        {
          "type": "label",
          "align": "top_left",
          "x": 30,
          "y": 65,
          "text": "%s%%",
          "topic": "topic/humidite"
        }
      ]
    }
  ]
}
```
Le fichier commence par une liste de topics mqtt et ensuite d'une liste d'objets qui vont décrire l'affichage de ses topics. Chaque objet que nous allons décrire pourra contenir lui même une liste d'objets enfants. Dans l'exemple l'objet de type *tab* contient deux objets de type *label*.

Avec un fichier pluscomplet ça donne ceci

![snapshot](/images_readme/snapshot.png "snapshot")

# Topics

L'objet *mqtt* contient une liste d'objets *topic* auquel le programme va souscrire. Dans notre exemple *topic/temperature* et *topic/humidite*.

# Objets

L'objet *objects* contient une liste de widgets qui seront affichés. Le premier est de type *tab* et permet de créer un onglet qui sera intitulé *Salon*. Cet onglet contient lui même une liste d'objets, deux dans notre exemple qui sont de type *label*. *align*, *x* et *y* permettent de définir la position de l'objet dans le parent. *text* renseigne le texte qui sera affiché, dans notre cas le texte contient %s ce qui permettra d'insérer le topic que l'on désire afficher, ici *topic/temperature* et *topic/humidite* que l'on doit bien évidemment retrouver dans la liste des topics mqtt.

### Type tab

Attribut *text* qui indique le titre de l'onglet et qui est le seul attribut de ce widget

```
"type": "tab",
"text": "Chauffage",
"objects": 
[
  {
  }
]
```

![tab](/images_readme/tab.png "tab")

### Type panel

Attributs *x*, *y* et *align* qui vont indiquer la position de l'objet à l'intérieur de son objet parent. *align* peut valoir *center*, *top_left*, *top_mid*, *top_right*, *bottom_left*, *bottom_mid*, *bottom_right*, *left_mid* et *right_mid*.

Par exemple *top_left* va positionner le bord haut de l'objet sur le bord haut du parent et le bord gauche de l'objet sur le bord gauche du parent. *x* et *y* indiqueront un déplacement par rapport à cette position. Si *x* vaut 10 l'objet se déplacera de 10 pixels vers la droite.

Tandis que *bottom_right* va positionner le bord bas de l'objet sur le bord bas du parent et le bord droit de l'objet sur le bord droite du parent. Pour décaler l'objet de 5 pixels vers la gauche on utilisera une valeur de *x* à -5;

Les attributs *w* et *h* indiqueront la taille de l'objet ( width et height).

```
"type": "panel",
"x": 0,
"y": 0,
"w": 290,
"h": 70,
"objects": 
[
  {
  }
]
```
![panel](/images_readme/panel.png "panel")

### Type label

Attributs *x*, *y*, *align*, *w* et *h*.
Attribut *text* indique le texte qui sera affiché.
Attribut *topic* indique que la valeur du topic spécifié devra être inséré dans le texte, le texte devra contenir %s pour indiquer l'endroit de l'insertion
Attribut *color* indique la couleur du texte
Attribut *size* indique la taille du texte si rien n'est indiqué c'est petit sinon on peut indiquer *medium*ou *large*

```
"type": "label",
"align": "center",
"x": 0,
"y": 0,
"text": "%s",
"topic": "topic/programme_traduit",
"color": "blue",
"size": "medium",
"topicCondition": "topic/programme_active",
"conditions": 
[
  {
    "value": "normal",
    "color": "orange"
  },
  {
    "value": "comfort",
    "color": "red"
  }
]
```
Vous remarquerez les attributs *topicCondition* et*conditions* qui seront décrits plus tard.

![label](/images_readme/label.png "label")

### Type button

Attributs *x*, *y*, *align*, *w* et *h*. Typiquement ce widget devra contenir un autre widget de type *label* ou *image*
Attribut *topic* indique un topic qui sera publié avec la valeur 1 lors d'un clic sur le bouton.

```
"type": "button",
"align": "top_right",
"x": 0,
"y": 0,
"w": 80,
"h": 80,
"topic": "topic/demande_eau",
"objects": 
[
  {
    "type": "image",
    "x": 0,
    "y": 0,
    "align": "center",
    "name": "A:/bath_active.png",
    "topicCondition": "topic/forcer_eau_chaude",
    "conditions": 
    [
      {
        "value": "0",
        "name": "A:/bath.png"
      },
      {
        "value": "1",
        "name": "A:/bath_active.png"
      }
    ]
  }
]
```

Ici avec une image

![button](/images_readme/button.png "button")

### Type image

Attributs *x*, *y*, *align*, *w* et *h*.
Attribut *name* qui donnera le nom du fichier à afficher ( obligatoirement un fichier .png)

```
"type": "image",
"x": 0,
"y": 0,
"align": "center",
"text": "%s",
"name": "A:/bath_active.png"
```

### Type arc

Attributs *x*, *y*, *align*, *w* et *h*.
Attribut *size* pour la taille du label qui sera automatiquement ajouté au widget *arc*
Attributs *min* et *max* qui définissent les valeurs mini et maxi de l'arc
Attributs *text* et *topic* pour l'affichage de la valeur

![arc](/images_readme/arc.png "arc")

### Type chart

Attributs *x*, *y*, *align*, *w* et *h*.
Attributs *min* et *max* qui définissent les valeurs mini et maxi de l'arc
Attribut *points_count* qui indique le nombre de points du graphique
Le topic devra être constitué d'une liste de valeurs séparés par une virgule

```
"type": "chart",
"align": "center",
"x": 0,
"y": 0,
"w": 600,
"h": 300,
"points_count": 6,
"min": 0,
"max": 200,
"topic": "topic/conso_hebdo_gaz",
"objects": 
[
  {
    "type": "label",
    "align": "top_mid",
    "x": 0,
    "y": 0,
    "text": "m3/semaine"
  }
]
```

### Conditions

Il est possible de modifier les attributs d'un widget en fonction de la valeur d'un topic. Pour cela on indique un attribut *topicCondition* et une liste de *conditions*, dans l'exemple selon la valeur du topic *forcer_eau_chaude* on choisit le nom du fichier image qui sera affiché.

La condition peut-être *value*, *greater* et *less*

```
"topicCondition": "topic/forcer_eau_chaude",
"conditions": 
[
  {
    "value": "0",
    "name": "A:/bath.png"
  },
  {
    "value": "1",
    "name": "A:/bath_active.png"
  }
]
```

![chart](/images_readme/chart.png "chart")

# Composants tiers

    ArduinoJson de Benoit Blanchon
    Esp32-wifi-manager de Jack Huang

# License
*MQTT Jeedom Display* is MIT licensed. As such, it can be included in any project, commercial or not, as long as you retain original copyright. Please make sure to read the license file.
