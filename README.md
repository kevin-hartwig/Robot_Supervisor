#__*ERE-EL Platform Repository*__

*Authors: James Sonnenberg, Kevin Hartwig and Ovi Ofrim*

##Repo Info

Hello and welcome to the repository for the Platform Supervisor.  See the [documentation repository](https://github.com/oovi77/Robot-Documentation-) for additional project information. 

This repository is home to all of the code for controlling the platform supervisor from a PC running Linux.  

##Usage Instructions

Once you clone the repository, follow the steps below to get it running:

1. a. If using the USB port (i.e., using a serial-USB adapter), enter the following command in the Linux terminal:   

          > *sudo chmod 666 /dev/ttyUSB0*

   b. If instead you are using the COM port (DB9 connector connected directly to the PC), enter the following command in the linux terminal: 

          > *sudo chmod 666 /dev/ttyS0*

3. Run the executable titled "host"

4. Select the menu item "Command Mode"

5. Enter commands to control the robot.

6. Type "quit" at any time to go back to the main menu.

7. Press "ESC" to quit program.

###Related Project Repositories:
 + [Project Documentation](https://github.com/oovi77/Robot-Documentation-)
 + [Platform Controller](https://github.com/kevin-hartwig/Robot_Platform)

###Contact: 

* Kevin Hartwg:       khartwig8282@conestogac.on.ca
* James Sonnenberg    
* Ovi Ofrim           
