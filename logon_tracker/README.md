# Protospace Logon Tracker

This program is installed on various computers and tracks users who log on and POSTs it to the portal.

## Windows 7 file location:

`C:\ProgramData\Microsoft\Windows\Start Menu\Programs`

## Installation Procedure (windows 10)
### Deployed via GPO ###
1. A GPO has been created called "Copy Files, Install Fonts" which is run on all PC's.  <br>
  This GPO, in addition to other things, copies files to C:\Windows\psfiles.
2. Either copy the *.lnk file to **c:\ProgramData\Microsoft\Windows\Start Menu\Programs\StartUp**<br>
  or create a new shortcut that runs %windir%\psfiles\logon_tracker.cmd minimized.   

### Deployed Manually (If GPO is not working properly) ### 
Note that these procedures need to run with a logon ID that has local administrator previledges.
1. Create the directory **%windir%\PSFiles** on the PC   
2. Copy the files from repository to the above directory or the root directory of C:\\.  
  That is: Logon_Tracker.cmd, Logon_Tracker.PS1, and Logon_Tracker_shortcut.lnk
3. Move the file Logon_Tracker_shortcut.lnk to the following directory:
  **c:\ProgramData\Microsoft\Windows\Start Menu\Programs\StartUp**<br>
  This file simply launches the command file "C:\PSFiles\Logon_Tracker.cmd" <br>
  Ensure that the properties of the shortcut include running the program Minimized.
