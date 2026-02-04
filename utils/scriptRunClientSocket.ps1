# Define the path to your executable
param(
    [int]$Count = 10
)
$exePath = "D:\Development\C++\chatapp\build-debug\clientsocket\clientSocket.exe"

# Loop 10 times
for ($i = 1; $i -le $Count; $i++) {
    $fileName = "ClientSocket$i.log"
    Write-Host "Running instance $i and creating $fileName..."

    Start-Process -FilePath $exePath `
        -ArgumentList @("localhost", "8080", $fileName) `
        -RedirectStandardOutput "run.log" `
        -NoNewWindow #-Wait
    #Start-Sleep -Seconds 1
}

# Kill by process name
##Stop-Process -Name "YourProgram" -Force

# Kill by process ID
##Stop-Process -Id 1234 -Force