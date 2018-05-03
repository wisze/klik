# huisswitch

Huisswitch is an application to switch domestic appliances (lights mostly) on and off using klikaanklikuit socket devices. These get their commands over 433MHz.

The wemos version has a website with a number of buttons to switch lights on and off.

Timed on and off switch commands are hardcoded. At given times the switch on and off commands are sent automatically. The timed light on commands are sent only when the light resistor indicates it is dark in the room.
