# Event_Driven_Chat_Server
Lior Zadah
Ex4 - Chat Server
chatServer.c - implement the connection between the main terminal and the other's 
In This project i implementet a chatServer. i can send messeges between several terminals. like a whatsap group. 
In this program, i will implement an event-driven chat server. The function of the
chat is to forward each incoming message’s overall client connections (i.e., to all
clients) except for the client connection over which the message was received. The
challenge in such a server lies in implementing this behavior in an entirely eventdriven manner (no threads).
In this program, i’ll use “select” to check from which socket descriptor is ready for
reading or writing. You should call “select” inside a loop, but it should appear only
once in your exercise. 
