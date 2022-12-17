# AtomOS rendering service

Provides a layer ontop of the OS which applications or the OS can use to render UIs. It prevents the need for multiple rendering systems, each one competing for OS resources. 

In the future, it will come with its own UI library. This will allow other applications and the OS to easily (relativley, it is still C) create UIs with a unified intermediate representation. 

The render service handles optimisation of UIs (e.g not rendering off-screen objects) and the conversion of view instructions to framebuffers. 

### Current State 
Features implemented: 
- Displaying stuff 
- Multitouch 

Todo: 
- Render pipeline 
- UI Library 
