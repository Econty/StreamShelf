param([string]$HostPath="$env:LOCALAPPDATA\StreamShelfNative\connector\StreamShelf.Host.exe")
$ErrorActionPreference='Stop'
$info=[Diagnostics.ProcessStartInfo]::new($HostPath)
$info.UseShellExecute=$false
$info.CreateNoWindow=$true
$info.RedirectStandardInput=$true
$info.RedirectStandardOutput=$true
$info.RedirectStandardError=$true
$info.ArgumentList.Add('chrome-extension://cjhiagnpgpmaagnoinjnkipimjlplcpe/')
$process=[Diagnostics.Process]::new()
$process.StartInfo=$info
function Send-Frame($message){
 $bytes=[Text.Encoding]::UTF8.GetBytes(($message | ConvertTo-Json -Compress -Depth 8))
 $process.StandardInput.BaseStream.Write([BitConverter]::GetBytes([uint32]$bytes.Length))
 $process.StandardInput.BaseStream.Write($bytes)
 $process.StandardInput.BaseStream.Flush()
}
function Read-Frame([int]$timeout=90000){
 $header=[byte[]]::new(4)
 $read=$process.StandardOutput.BaseStream.ReadExactlyAsync($header).AsTask()
 if(!$read.Wait($timeout)){throw 'Timed out waiting for application command'}
 $size=[BitConverter]::ToUInt32($header)
 if($size -eq 0 -or $size -gt 1048576){throw 'Invalid frame size'}
 $body=[byte[]]::new($size)
 if(!$process.StandardOutput.BaseStream.ReadExactlyAsync($body).AsTask().Wait(5000)){throw 'Timed out reading frame body'}
 return ([Text.Encoding]::UTF8.GetString($body) | ConvertFrom-Json)
}
try{
 [void]$process.Start()
 Send-Frame @{type='hello';protocol=1;version='1.0.0';instanceId=[Guid]::NewGuid().ToString();browser='StreamShelf local regression test'}
 $reply=Read-Frame 5000
 if($reply.type -ne 'helloAck'){throw 'Handshake failed'}
 Send-Frame @{type='tabSelected';tabId=424242;url='https://example.org/streamshelf-regression';title='Локальная проверка соединения'}
 Write-Output 'READY: diagnostic session connected; waiting for UI scan command'
 $start=Read-Frame
 if($start.type -ne 'scanStart' -or $start.tabId -ne 424242){throw 'Unexpected scan command'}
 Send-Frame @{type='scanState';scanning=$true;tabId=424242}
 Write-Output 'PASS live application delivered scanStart through native host'
 $stop=Read-Frame
 if($stop.type -ne 'scanStop'){throw 'Unexpected stop command'}
 Send-Frame @{type='scanState';scanning=$false;tabId=424242}
 Write-Output 'PASS live application delivered scanStop; UI remained responsive'
}finally{
 if($process.Id){$process.StandardInput.Close();if(!$process.WaitForExit(3000)){$process.Kill($true)}}
 $process.Dispose()
}
