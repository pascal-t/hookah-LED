# hookah-LED
LED underglow for my hookah (in Germany we call it Shisha)

# Funktionen

## Einfarbig
* Langer Knopfdruck wechselt zu Regenbogen.
* Drehen des Drehencoders stellt die Farbe ein (ändert den Hue im Uhrzeigersinn).
* Kurzer Knopfdruck wechselt die Funktion des Drehencoders zu Sättigung (mehr Weiß/Pastell).
* Ein erneuter kurzer Knopfdruck wechselt die Funktion des Drehencoders zu Helligkeit (Value).
* Ein erneuter kurzer Knopfdruck wechselt die Funktion des Drehencodes, um das Mikrofon einzuschalten.
  * Aktivierung des Mikrofons lässt einzelne Pixel aufhellen (Sättigung runter, Helligkeit hoch) und langsam wieder in die Ursprungsposition zurückkehren.

## Regenbogen
* Langer Knopfdruck wechselt zu Weiß.
* Drehen des Drehencoders ändert die Drehgeschwindigkeit des Regenbogens.
* Ein kurzer Knopfdruck wechselt die Funktion des Drehencoders zu Helligkeit.
* Ein erneuter kurzer Knopfdruck wechselt die Funktion des Drehencoders, um das Mikrofon einzuschalten.
  * Aktivierung des Mikrofons lässt den Regenbogen schneller drehen.

## Weiß
* Langer Knopfdruck wechselt zu einfarbig.
* Drehen des Drehencoders ändert die Helligkeit.
* Ein kurzer Knopfdruck wechselt die Funktion des Drehencoders, um das Mikrofon einzuschalten.
  * Aktivierung des Mikrofons lässt einzelne Pixel in zufälligen Farben aufhellen und langsam wieder in die Ursprungsposition zurückkehren.

## Einstellungen
* Die Funktion wird durch weiß blinkende LEDs angezeigt, die in ihrer Anzahl dem Index der Funktion entsprechen.
  * Standardfunktion: Keine LED blinkt.
  * 1\. Funktion (z. B. Sättigung bei Farbe): 1 LED blinkt.
  * Usw ...
* Um den Status des Mikrofons anzuzeigen, werden alle LEDs - außer die blinkenden - ausgeschaltet.
  * Wenn die mittlere LED grün leuchtet, ist das Mikrofon an, wenn sie rot leuchtet nicht.
* Lange Knopfdrücke sind 250 ms lang.
* Der verwendete Drehencoder hat 20 Schritte/ Umdrehung.
  * evtl. eine grobe Farbeinstellung und eine feinere (separiert durch einen Knopfdruck wie andere Einstellungen)?
