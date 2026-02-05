# TailChatServerLog.ps1
# This script tails the last 10 lines of the chat server log file and waits for new entries.
Get-Content -Path "D:\Development\C++\chatapp\log\chatServer.log" -Tail 10 -wait
#List installed applications
Get-StartApps | Select Name > InstalledApps.txt

# Kill by process name
Stop-Process -Name "YourProgram" -Force

# Kill by process ID
Stop-Process -Id 1234 -Force