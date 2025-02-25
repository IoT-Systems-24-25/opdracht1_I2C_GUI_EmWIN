 STM32F4 I2C & GUI met emWin

Dit project maakt gebruik van een **STM32F429I-DISC1** microcontroller om een **externe temperatuur sensor (TC74A0)** en de **interne temperatuur sensor van de MCU** uit te lezen. De verkregen data wordt weergegeven via een **GUI op een touchscreen (emWin)** en verzonden via **UART**. 

Een **ventilator (gesimuleerd met LED LD3)** schakelt automatisch in bij een ingestelde temperatuurgrens, en gebruikers kunnen de temperatuur loggen naar een **USB-stick**.

---

 **Functionaliteiten**
 **Temperatuur uitlezen** van een **externe TC74A0 sensor** via **I2C**.
 **Interne temperatuur uitlezen** van de **STM32F4 MCU** via **ADC**.
 **Temperatuur weergeven** op een **touchscreen (emWin GUI)**.
 **Handmatige en automatische ventilatorregeling** via een **slider** en knoppen.
 **Gegevens opslaan naar USB** in een TXT-bestand.
 **Live logging van temperatuur via UART** voor debug-doeleinden.

---

**Benodigde hardware**
- **STM32F429I-DISC1** ontwikkelbord
- **TC74A0 temperatuur sensor** (I2C)
- **Micro-USB kabel** voor programmering en debugging
- **USB-stick** (optioneel, voor logging)
- **LED (LD3) als simulatie voor een ventilator**

---

**Gebruikte tools & software**
- **Keil \u00b5Vision** (voor codeontwikkeling)
- **STM32CubeMX** (voor hardwareconfiguratie)
- **Git & GitHub** (voor versiebeheer)
- **Putty / Tera Term** (voor UART-debugging)
- **emWin GUI-Builder** (voor de touch interface)

---
**Installatie & Setup**
**1️⃣ STM32CubeMX configureren**
1. Open STM32CubeMX en laad de **hardwareconfiguratie** (`.ioc` file).
2. Stel de volgende **peripherals** in:
   - **I2C3** voor de **TC74A0 temperatuur sensor**.
   - **ADC1** voor de **interne temperatuur sensor**.
   - **USB Host** voor **USB logging**.
   - **UART1** voor **seriële communicatie**.
3. Genereer de code en open in **Keil \u00b5Vision**.

---

**2️⃣ Code compileren en uploaden**
1. Clone deze repository:
   ```sh
   git clone https://github.com/IoT-Systems-24-25/opdracht1_I2C_GUI_EmWIN.git
   ```
2. Open het project in **Keil \u00b5Vision**.
3. **Compileer en laad de code op de STM32** via ST-Link.

---

**Hoe het werkt**
1. **Start de STM32** → GUI verschijnt op het **touchscreen**.
2. **Klik op "Toon Temperatuur"** → Externe en interne temperatuur worden uitgelezen en getoond in de GUI.
3. **Schuif de slider** → Wijzigt de temperatuurgrens voor het inschakelen van de ventilator.
4. **Klik op "AUTO"** → Ventilator (LED LD3) schakelt **automatisch** in bij overschrijding van de temperatuurgrens.
5. **Klik op "MANUAL"** → Gebruiker kan de ventilator **handmatig** in- en uitschakelen.
6. **Klik op "Save naar USB"** → De meetwaarden worden **gelogd naar een USB-stick**.

---


**Contact**
Heb je vragen of verbeteringen? Maak een **issue** aan of doe een **pull request**!  

🔗 **GitHub Repository**:
[https://github.com/IoT-Systems-24-25/opdracht1_I2C_GUI_EmWIN.git](https://github.com/IoT-Systems-24-25/opdracht1_I2C_GUI_EmWIN.git)





