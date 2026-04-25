# Schéma de Câblage Complet - Robot IA Local

Ce document détaille les connexions matérielles pour l'ESP32, l'écran ST7789, l'audio I2S et le tactile.

## 1. Alimentation (Power)
* **ESP32 VIN** : Connecté à la sortie 5V du régulateur Step-down.
* **MAX98357A VIN** : Connecté au 5V (consomme beaucoup de courant lors des pics audio).
* **GND** : Tous les GND (ESP32, Micro, HP, Écran) doivent être reliés ensemble.

## 2. Bus SPI Partagé (Données)
Trois composants partagent les fils de données. Reliez-les en "bus" sur votre plaque d'essai :
* **MOSI (Données sortantes)** : ESP32 **GPIO 23** -> Vers SDI(Écran), MOSI(SD), T_DIN(Tactile).
* **MISO (Données entrantes)** : ESP32 **GPIO 19** -> Vers SDO(SD), T_DO(Tactile).
* **SCK (Horloge)** : ESP32 **GPIO 18** -> Vers SCL(Écran), SCK(SD), T_CLK(Tactile).

## 3. Sélections Individuelles (Chip Select & Commandes)
Chaque composant possède ses propres fils de contrôle :
* **Écran TFT CS** : **GPIO 14**
* **Écran DC** : **GPIO 21**
* **Écran RST** : **GPIO 4**
* **Carte SD CS** : **GPIO 5**
* **Tactile T_CS** : **GPIO 13**

## 4. Audio I2S
### Entrée (Microphone INMP441)
* **WS** : **GPIO 25**
* **SCK** : **GPIO 32**
* **SD** : **GPIO 33**
* **L/R** : GND

### Sortie (Haut-Parleur MAX98357A)
* **LRC (WS)** : **GPIO 26**
* **BCLK** : **GPIO 27**
* **DIN** : **GPIO 22**

---
*Dernière mise à jour : 23 Avril 2026*
