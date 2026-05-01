# 🌊 Visualiseur LED pour ELEGOO Neptune 4 Max

**Version stable : 1.4.0 FR**  
*Auteur : Israel Garcia Armas avec DeepSeek*

![Aperçu du visualiseur LED](images/led_visualizer_action.jpg)

---

## 📖 Origine du projet

Ce projet est l'évolution naturelle d’un système de visualisation LED que j’ai développé pour mon imprimante **Snapmaker U1**. À l’époque, la Snapmaker U1 utilisait un **firmware Klipper propriétaire et verrouillé**, ce qui empêchait d’accéder à l’API standard de Moonraker. J’ai donc **modifié le firmware d’origine et l’ai étendu avec Klipper Extended** pour ouvrir la communication et accéder aux données de l’imprimante en temps réel. Cette première version a été un succès.

Fort de cette expérience, j’ai adapté le système à **Klipper standard** sur ma **ELEGOO Neptune 4 Max** (une imprimante grand format que j’avais déjà dans mon atelier). Le résultat est ce projet entièrement fonctionnel, robuste et facilement **portable sur n’importe quelle imprimante exécutant Klipper + Moonraker**.

Mon objectif est d’**adapter ce système à toutes mes imprimantes basées sur Klipper**, quel que soit le fabricant, dès lors qu’elles exposent l’API de Moonraker.

---

## 🎯 Objectifs principaux

- **Surveiller en temps réel** l’état de l’imprimante : repos, chauffage, impression (avec barre de progression), pause, terminé, erreur et calibration du plateau.
- **Interface web lisible et responsive** (mobile, tablette, PC) affichant les températures, la progression et permettant de configurer les effets de couleur.
- **Personnalisation complète des effets LED** pour chaque état (couleur fixe, respiration, clignotement, arc-en-ciel, vague) avec stockage dans la mémoire SPIFFS de l’ESP32.
- **Détection automatique de la calibration du plateau** (nivellement) grâce à une logique heuristique utilisant la température de la buse (140°C) et les mouvements des axes.
- **Animation de démarrage attractive** (deux serpents bleus qui se rencontrent au centre + flash de couleur par phase) informant sur l’état du système (WiFi, SPIFFS, Moonraker, succès final).

---

## 🧩 Composants et technologies

| Domaine | Composant / Technologie |
| :--- | :--- |
| **Microcontrôleur** | ESP32 (NodeMCU-32S ou similaire) |
| **Bande LED** | NeoPixel (WS2812B) – 21 LEDs (configurable) |
| **Firmware de l’imprimante** | Klipper + Moonraker (standard sur Neptune 4 Max) |
| **Librairies Arduino** | WiFiManager, ArduinoJson, Adafruit_NeoPixel, WebServer, SPIFFS |
| **Environnement de développement** | Arduino IDE 2.x |
| **Communication** | HTTP (JSON via Moonraker) |
| **Interface web** | HTML5, CSS3, JavaScript (fetch, manipulation dynamique) |

---

## ✨ Caractéristiques principales

### 📡 WiFiManager – Configuration WiFi simple et persistante

Le système utilise la bibliothèque **WiFiManager** pour gérer la connexion WiFi de manière totalement autonome, sans avoir à coder les identifiants en dur.

- **Premier démarrage** : l’ESP32 crée un point d’accès `Neptune4-Lights` (sans mot de passe). Vous vous y connectez et un portail captif vous permet de choisir votre réseau domestique et d’entrer votre mot de passe.
- **Stockage sécurisé** : les identifiants sont sauvegardés dans la mémoire flash de l’ESP32. Au redémarrage, la connexion est automatique.
- **Défaillance** : si la connexion échoue (mot de passe changé, etc.), l’ESP32 recrée le point d’accès pour une nouvelle configuration.

### 🚀 Démarrage (boot) visuel en 4 phases

1. **WiFi** – serpents bleus + flash bleu au centre de la bande.
2. **SPIFFS** – serpents bleus + flash jaune.
3. **Moonraker** – serpents bleus + flash magenta.
4. **Succès final** – 4 clignotements verts sur toute la bande.

### 🤖 États automatiques de l’imprimante

| État | Effet LED par défaut | Description |
| :--- | :--- | :--- |
| `idle` (repos) | respiration verte douce | Imprimante au repos, aucun fichier chargé. |
| `heating` (chauffage) | orange en respiration | Chauffage du lit ou de la buse. |
| `printing` (impression) | barre de progression bleue à respiration lente (4 s) | Impression en cours, la barre se remplit en fonction du pourcentage réel. |
| `paused` (pause) | jaune clignotant | Impression en pause. |
| `finished` (terminé) | arc-en-ciel (persiste 2 min) | Impression terminée. Annulable depuis l’interface web (boutons **REPOS** ou **MODE AUTO**). |
| `error` (erreur) | rouge clignotant | Erreur ou annulation (`error` / `cancelled`). |
| `calibrating` (calibration) | vague verte/bleue | Calibration du plateau détectée automatiquement (buse à 140 °C + mouvement des axes). |

### 🔧 Détection intelligente de la calibration du plateau

- **Déclenchement** : détection d’une température de buse à **140°C** (valeur exacte utilisée par l’imprimante pendant le nivellement).
- **Suivi** : surveillance du **mouvement sur les axes X, Y et Z** (seuil > 2 mm).
- **Fin de calibration** lorsque :
  - Plus de mouvement pendant **5 secondes** consécutives.
  - La température cible de la buse n’est plus 140°C.
  - **Timeout de sécurité** de 5 minutes (au cas où le mouvement ne s’arrêterait pas).

### 🌐 Interface web

- **Design clair et contrasté** (texte clair sur fond sombre, sans zones de texte sombres illisibles).
- **Affichage en temps réel** : état actuel, barre de progression, nom du fichier en cours, températures réelles et cibles (buse et lit).
- **Contrôle de la luminosité** (0–255).
- **Boutons manuels** pour forcer chaque état (mode manuel) et revenir en mode automatique.
- **Panneau de configuration des effets** :
  - Menu déroulant pour choisir le type d’effet (couleur fixe, respiration, clignotement, arc-en-ciel, vague).
  - Sélecteurs de couleur (primaire et secondaire pour la vague ou le fond de la barre).
  - Ajustement de la vitesse (ms par cycle).
  - Aperçu en direct, sauvegarde et restauration des valeurs par défaut.
- **Affichage automatique de la caméra** (si configurée sur l’imprimante) : l’ESP32 interroge Moonraker pour obtenir l’URL du flux vidéo et l’affiche dans une section dédiée.
- **Mentions de version** : numéro de version, étiquette « Beta », crédits de l’auteur.

### 🔄 État `finished` (terminé) persistant

- Après une impression, l’état `finished` reste affiché pendant **2 minutes**.
- Il peut être annulé manuellement depuis l’interface web avec les boutons **REPOS** ou **MODE AUTO**.
- Il s’annule également automatiquement si une nouvelle impression est lancée.

### 🛠️ Stockage de la configuration

- Les effets personnalisés sont sauvegardés dans un fichier `config.json` en SPIFFS.
- Au redémarrage de l’ESP32, la dernière configuration est automatiquement restaurée.

---

## ✅ Avantages et points forts

- **Aucune modification du firmware de l’imprimante** – fonctionne avec l’API Moonraker standard.
- **Entièrement configurable** (couleurs, effets, vitesses) depuis une interface web simple.
- **Réactivité en temps réel** (mise à jour toutes les 500 ms).
- **Installation facile** – connexion directe à l’ESP32, aucun composant supplémentaire.
- **Coût très bas** (environ 15–30 € de matériel).
- **Évolutif** – le nombre de LEDs est ajustable en modifiant une seule constante.
- **Portable** – l’ESP32 peut être alimenté via n’importe quel port USB, y compris celui de l’imprimante.
- **Compatible** avec toute imprimante équipée de Klipper + Moonraker (pas seulement la Neptune 4 Max).

---

## 📦 Matériel requis et coût estimé

| Composant | Coût approximatif |
| :--- | :--- |
| ESP32 (NodeMCU-32S) | 5–8 € |
| Bande LED NeoPixel (21 LEDs, WS2812B) | 5–10 € |
| Câbles et connectiques | 1–2 € |
| Alimentation 5V (optionnelle, si non alimentée par USB) | 5–10 € |
| **Total** | **15–30 €** |

---

## 🔒 Limitations et points d’amélioration actuels

- **Détection de calibration heuristique** : peut parfois donner des faux positifs (mouvements inhabituels), mais les seuils sont réglés pour minimiser ce risque.
- **Interface web non PWA** (ne peut pas être installée comme une application mobile, mais totalement fonctionnelle dans un navigateur).
- **Absence d’horodatage réel** (pas de RTC sur l’ESP32 ; les logs du moniteur série affichent l’heure de l’ordinateur).
- **Configuration stockée en SPIFFS** : une corruption peut survenir lors d’une coupure de courant extrême (dans ce cas, les valeurs par défaut sont restaurées).

---

## 🔮 Améliorations futures envisagées

- **Support de plus de 7 états** (personnalisables par l’utilisateur).
- **Détection directe de la commande `BED_MESH_CALIBRATE`** si Moonraker l’expose dans l’objet `history`.
- **Notifications push** (Telegram, Blynk, etc.) pour alerter de la fin d’impression ou d’une erreur.
- **Mode « simulation »** pour tester tous les états sans disposer d’une imprimante réelle.
- **Intégration avec Home Assistant** (MQTT ou API REST).
- **Ajout d’effets supplémentaires** (trainée, feu, éclair) sans surcharge de l’ESP32.
- **Traduction de l’interface web en plusieurs langues** (espagnol, français, anglais, allemand…).
- **Contrôle par bouton physique** pour basculer manuellement entre mode automatique et manuel.
- **Adaptation automatique à différentes imprimantes** (détection dynamique des axes, des limites, etc.).

---

## ⚙️ Installation et configuration

### 1. Installer les librairies nécessaires (Arduino IDE)

- `WiFiManager` (par tzapu)
- `ArduinoJson` (version 6)
- `Adafruit_NeoPixel`
- `WebServer` (inclus avec le noyau ESP32)
- `SPIFFS` (inclus)

### 2. Configurer la carte

- Sélectionner **ESP32 Dev Module** dans l’IDE Arduino.
- Choisir le port COM correct.

### 3. Téléverser le code

- Copier le code complet (version 1.4.0 FR) dans un nouveau croquis.
- Compiler et téléverser vers l’ESP32.

### 4. Premier démarrage (configuration WiFi)

- L’ESP32 crée un point d’accès `Neptune4-Lights` (sans mot de passe).
- Connectez-vous avec votre mobile ou PC à ce réseau.
- Un portail captif s’ouvre ; choisissez votre réseau WiFi domestique et entrez votre mot de passe.
- L’ESP32 redémarre et se connecte à votre réseau domestique.

### 5. Accéder à l’interface web

- Ouvrez le moniteur série (115200 bauds) pour connaître l’adresse IP de l’ESP32 (par exemple `192.168.1.39`).
- Dans un navigateur, allez sur `http://[IP_de_l'ESP32]`.
- Vous pouvez maintenant utiliser l’interface web.

---

## 📝 Utilisation

- **Mode automatique** : le système suit l’état réel de l’imprimante.
- **Mode manuel** : vous pouvez forcer n’importe quel état avec les boutons (les LEDs changent et la webcam s’affiche si disponible). Un timer de 5 secondes ramène au mode automatique par défaut (sauf si vous désactivez cette option).
- **Configuration des effets** : cliquez sur le bouton **⚙️ CONFIGURER LES EFFETS** pour afficher/masquer le panneau de configuration.

---

## 🤝 Contribution

Ce projet est **open source** et toute contribution est la bienvenue. Vous pouvez :

- Signaler un bug ou proposer une amélioration via les **Issues** GitHub.
- Envoyer une **Pull Request** pour corriger ou ajouter des fonctionnalités.
- Adapter le système à d’autres imprimantes Klipper (il suffit souvent de modifier l’adresse IP et quelques seuils).
- Améliorer l’interface web, ajouter des effets, affiner la détection de calibration, etc.

---

## 📄 Licence

Ce projet est distribué sous licence **MIT**, ce qui autorise son utilisation, sa copie, sa modification et sa distribution libre, à condition d’inclure l’avis de copyright et la licence originale.

---

## 🙏 Remerciements

- À la communauté **Klipper et Moonraker** pour leur API documentée et stable.
- Aux développeurs des librairies `WiFiManager`, `ArduinoJson` et `Adafruit_NeoPixel`.
- À **DeepSeek** pour l’assistance au débogage et au développement du code.

---

**Profitez de votre visualiseur LED et que toutes vos impressions soient réussies !** 🌊🖨️✨