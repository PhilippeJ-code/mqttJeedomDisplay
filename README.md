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
L'étape suivante consistera à créer un fichier display.json qui permettra de configure les topics utilisés et la façon de les afficher

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

## Topics



# Composants tiers

    ArduinoJson de Benoit Blanchon
    Esp32-wifi-manager de Jack Huang

# License
*MQTT Jeedom Display* is MIT licensed. As such, it can be included in any project, commercial or not, as long as you retain original copyright. Please make sure to read the license file.
