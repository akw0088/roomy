# roomy
So... about that roomy idea. This is the remote desktop app, it still needs work, but now supports the mouse and keyboard, but be careful as the screen updates slightly slowly. Started with just the idea of streaming webcam bitmaps being pretty similar to streaming desktop bitmaps. Getting a viewer working wasn't too bad with the initial code from zoomy. Once that worked decently I added the mouse support first, which was a bit of effort, but not the end of the world. I originally wasn't going to add the keyboard, but actually the keyboard was mind blowingly easy to add (just a keycode and a up/down state). Run the binary in server mode on the target machine with listen enabled, then on a different machine, mark server and listen off and set the ip to the comptuer running the server. It should then connect and start displaying the remote machines desktop. Be sure to allow the app through the windows firewall too, the server can run hidden, so kill it in task manager if you started it and it disappeared. You can setup the server to connect out and the client to listen if you'd like. Feel free to try it out, but not quite as nice as VNC or RDP (remote desktop) yet...