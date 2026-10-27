param([string]$Executable = '.\build\Release\GameClient.exe', [switch]$Expanded)

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class GameWindowInput {
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X; public int Y; }
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window, ref Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint x, uint y, uint data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, UIntPtr wParam, IntPtr lParam);
}
'@

$exe = (Resolve-Path -LiteralPath $Executable).Path
$outputDir = Split-Path -Parent $exe
$process = Start-Process -FilePath $exe -PassThru -WindowStyle Normal
try {
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 100 -and $window -eq [IntPtr]::Zero; ++$attempt) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        if ($process.HasExited) { throw "Game exited before opening its window: $($process.ExitCode)" }
        $candidate = [GameWindowInput]::FindWindow('EasileGameClientWindow','EASILE GameClient')
        $ownerId = [uint32]0
        [GameWindowInput]::GetWindowThreadProcessId($candidate,[ref]$ownerId) | Out-Null
        if ($ownerId -eq $process.Id) { $window = $candidate }
    }
    if ($window -eq [IntPtr]::Zero) { throw 'Game window did not open' }
    if (!$process.WaitForInputIdle(10000)) { throw 'Game input loop did not become ready' }
    [GameWindowInput]::ShowWindow($window, 9) | Out-Null
    [GameWindowInput]::SetWindowPos($window, [IntPtr]::Zero, 40, 40, 1304, 759, 0x0040) | Out-Null
    [GameWindowInput]::SetForegroundWindow($window) | Out-Null
    Start-Sleep -Milliseconds 300
    if ([GameWindowInput]::GetForegroundWindow() -ne $window) {
        $activation = New-Object -ComObject WScript.Shell
        $activation.AppActivate($process.Id) | Out-Null
        [GameWindowInput]::keybd_event(18,0,0,[UIntPtr]::Zero)
        [GameWindowInput]::SetForegroundWindow($window) | Out-Null
        [GameWindowInput]::keybd_event(18,0,2,[UIntPtr]::Zero)
        Start-Sleep -Milliseconds 300
    }
    if ([GameWindowInput]::GetForegroundWindow() -ne $window) { throw 'Game window could not receive foreground input' }
    $client = New-Object GameWindowInput+Rect
    [GameWindowInput]::GetClientRect($window, [ref]$client) | Out-Null
    if ($client.Right -le 0 -or $client.Bottom -le 0) { throw 'Game client area is empty' }
    $origin = New-Object GameWindowInput+Point
    [GameWindowInput]::ClientToScreen($window, [ref]$origin) | Out-Null
    $cursorX = $origin.X + [int]($client.Right * 400 / 1280)
    $cursorY = $origin.Y + [int]($client.Bottom * 360 / 720)
    [GameWindowInput]::SetCursorPos($cursorX, $cursorY) | Out-Null

    function Tap-Key([byte]$key) {
        Ensure-Foreground
        [GameWindowInput]::keybd_event($key, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 90
        [GameWindowInput]::keybd_event($key, 0, 2, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 120
    }
    function Tap-Click {
        Ensure-Foreground
        [GameWindowInput]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 90
        [GameWindowInput]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 120
    }
    function Capture([string]$name) {
        Ensure-Foreground
        [GameWindowInput]::SetProcessDPIAware() | Out-Null
        $bounds = New-Object GameWindowInput+Rect
        [GameWindowInput]::GetWindowRect($window, [ref]$bounds) | Out-Null
        $width = $bounds.Right - $bounds.Left
        $height = $bounds.Bottom - $bounds.Top
        $bitmap = New-Object System.Drawing.Bitmap($width, $height)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try { $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size) }
            finally { $graphics.Dispose() }
            $bitmap.Save((Join-Path $outputDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $bitmap.Dispose() }
    }
    function Hold-Key([byte]$key,[int]$milliseconds) {
        Ensure-Foreground
        [GameWindowInput]::keybd_event($key,0,0,[UIntPtr]::Zero)
        try { Start-Sleep -Milliseconds $milliseconds }
        finally { [GameWindowInput]::keybd_event($key,0,2,[UIntPtr]::Zero) }
    }

    function Ensure-Foreground {
        if (![GameWindowInput]::IsWindow($window)) { throw 'Game window closed during verification' }
        if ([GameWindowInput]::IsIconic($window)) {
            [GameWindowInput]::ShowWindow($window,9) | Out-Null
            Start-Sleep -Milliseconds 200
        }
        if ([GameWindowInput]::GetForegroundWindow() -eq $window) { return }
        [GameWindowInput]::SetForegroundWindow($window) | Out-Null
        if ([GameWindowInput]::GetForegroundWindow() -ne $window) {
            $activation = New-Object -ComObject WScript.Shell
            $activation.AppActivate($process.Id) | Out-Null
            [GameWindowInput]::keybd_event(18,0,0,[UIntPtr]::Zero)
            [GameWindowInput]::SetForegroundWindow($window) | Out-Null
            [GameWindowInput]::keybd_event(18,0,2,[UIntPtr]::Zero)
            Start-Sleep -Milliseconds 200
        }
        if ([GameWindowInput]::GetForegroundWindow() -ne $window) {
            throw 'Game lost foreground input during verification'
        }
    }

    Tap-Key 13
    $matchClock = [System.Diagnostics.Stopwatch]::StartNew()
    Capture 'gui-input-start.png'
    Capture 'bot-after.png'
    Tap-Click
    Tap-Click
    Capture 'gui-input-clicks.png'
    Tap-Key 75
    [GameWindowInput]::keybd_event(75, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 550
    [GameWindowInput]::keybd_event(75, 0, 2, [UIntPtr]::Zero)
    Capture 'gui-input-k-held.png'
    Tap-Key 74
    Capture 'gui-input-auto.png'
    Tap-Key 9
    Tap-Key 75
    Capture 'gui-input-mouse.png'
    if ($Expanded) {
        Hold-Key 68 3300
        Capture 'v014-gui-camera-move.png'
        Hold-Key 87 650
        Hold-Key 68 1500
        Hold-Key 87 300
        Capture 'v014-gui-jungle-entry.png'
        Hold-Key 83 650
        Hold-Key 65 120
        foreach ($key in @(37,38,39,40)) { Hold-Key ([byte]$key) 120 }
        Tap-Key 80
        Capture 'v014-gui-paused.png'
        Tap-Key 80
        Tap-Key 74
        Capture 'v014-gui-mouse-j.png'
        Write-Output 'Expanded GUI inputs: WASD, all Arrow Keys, P pause/resume, Mouse J'
    }
    [GameWindowInput]::SetWindowPos($window, [IntPtr]::Zero, 80, 80, 900, 600, 0x0040) | Out-Null
    Start-Sleep -Milliseconds 250
    [GameWindowInput]::GetClientRect($window, [ref]$client) | Out-Null
    if ($client.Right -le 0 -or $client.Bottom -le 0) { throw 'Resized game client area is empty' }
    $resizedOrigin = New-Object GameWindowInput+Point
    [GameWindowInput]::ClientToScreen($window, [ref]$resizedOrigin) | Out-Null
    [GameWindowInput]::SetCursorPos($resizedOrigin.X + [int]($client.Right * 640 / 1280),
                                    $resizedOrigin.Y + [int]($client.Bottom * 360 / 720)) | Out-Null
    Start-Sleep -Milliseconds 120
    Capture 'gui-input-resized.png'
    Tap-Click
    Write-Output "Resized client: $($client.Right)x$($client.Bottom), Mouse-mode center click sent"
    if ($env:EASILE_MAP_EVIDENCE -eq '1') {
        [GameWindowInput]::SetWindowPos($window, [IntPtr]::Zero, 80, 80, 1304, 759, 0x0040) | Out-Null
        Start-Sleep -Milliseconds 250
        Capture 'map-connected.png'
        while ($matchClock.Elapsed.TotalSeconds -lt 6.0) { Start-Sleep -Milliseconds 100 }
        for ($sample = 0; $sample -lt 35; ++$sample) {
            Capture ('map-tower-{0:D2}.png' -f $sample)
            Start-Sleep -Milliseconds 120
        }
        while ($matchClock.Elapsed.TotalSeconds -lt 20.05) { Start-Sleep -Milliseconds 20 }
        Capture 'map-pickup.png'
        while ($matchClock.Elapsed.TotalSeconds -lt 35.0) { Start-Sleep -Milliseconds 100 }
        Capture 'map-late-siege.png'
        Write-Output "Map GUI evidence captured at $($matchClock.Elapsed.TotalSeconds.ToString('F1')) seconds"
    }
    Write-Output "GUI input sent: Enter, LMB x2, K, K hold, J, Tab, K"
    Write-Output "Client: $($client.Right)x$($client.Bottom), cursor screen: $cursorX,$cursorY"
} finally {
    if (!$process.HasExited -and [GameWindowInput]::IsWindow($window)) {
        [GameWindowInput]::PostMessage($window, 0x0010, [UIntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        $process.WaitForExit(3000) | Out-Null
    }
}
