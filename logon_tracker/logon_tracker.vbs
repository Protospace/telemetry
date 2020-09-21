' This script tracks a computer login for Spaceport
' Written by Tanner
' Problems: protospace@tannercollin.com

Dim computer_name
computer_name = CreateObject("WScript.Network").ComputerName

Dim user_name
user_name = CreateObject("WScript.Network").UserName

While True
    WScript.Sleep(10000)

    Dim request
    Set request = CreateObject("MSXML2.XMLHTTP")
    request.open "POST", "https://api.spaceport.dns.t0.vc/stats/track/", False
    request.setRequestHeader "Content-type", "application/x-www-form-urlencoded"
    request.send "name=" & computer_name & "&username=" & user_name

    Set request = CreateObject("MSXML2.XMLHTTP")
    request.open "POST", "https://api.my.protospace.ca/stats/track/", False
    request.setRequestHeader "Content-type", "application/x-www-form-urlencoded"
    request.send "name=" & computer_name & "&username=" & user_name
Wend
