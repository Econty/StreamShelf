param([Parameter(Mandatory=$true)][int]$Port)
$ErrorActionPreference='Stop'
$info=[Diagnostics.ProcessStartInfo]::new("$env:LOCALAPPDATA\StreamShelfNative\connector\StreamShelf.Host.exe")
$info.UseShellExecute=$false;$info.CreateNoWindow=$true
$info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
$info.ArgumentList.Add('chrome-extension://cjhiagnpgpmaagnoinjnkipimjlplcpe/')
$process=[Diagnostics.Process]::new();$process.StartInfo=$info
function Send-Frame($message){
 $bytes=[Text.Encoding]::UTF8.GetBytes(($message | ConvertTo-Json -Compress -Depth 10))
 $process.StandardInput.BaseStream.Write([BitConverter]::GetBytes([uint32]$bytes.Length))
 $process.StandardInput.BaseStream.Write($bytes);$process.StandardInput.BaseStream.Flush()
}
function Read-Frame {
 $header=[byte[]]::new(4)
 if(!$process.StandardOutput.BaseStream.ReadExactlyAsync($header).AsTask().Wait(120000)){throw 'Test session timed out'}
 $size=[BitConverter]::ToUInt32($header);if($size -gt 1048576){throw 'Invalid frame'}
 $body=[byte[]]::new($size)
 if(!$process.StandardOutput.BaseStream.ReadExactlyAsync($body).AsTask().Wait(5000)){throw 'Read timed out'}
 [Text.Encoding]::UTF8.GetString($body) | ConvertFrom-Json
}
try{
 [void]$process.Start()
 Send-Frame @{type='hello';protocol=1;version='1.0.3';instanceId=[Guid]::NewGuid().ToString();browser='Локальная проверка миниатюр'}
 $reply=Read-Frame;if($reply.type -ne 'helloAck'){throw 'Handshake failed'}
 $base="http://127.0.0.1:$Port"
 Send-Frame @{type='tabSelected';tabId=424243;url="$base/";title='ТЕСТ — собственные локальные изображения и видео'}
 $items=@(
  @{url="$base/test.png";kind='Изображения';thumbnail="$base/test.png";title='Тест — красное изображение PNG';source='Локальный тест'},
  @{url="$base/test.jpg";kind='Изображения';thumbnail="$base/test.jpg";title='Тест — красное изображение JPEG';source='Локальный тест'},
  @{url="$base/test.mp4";kind='Видео';title='Тест — цветное видео, кадр FFmpeg';source='Локальный тест'}
 )
 foreach($item in $items){Send-Frame @{type='resource';tabId=424243;pageUrl="$base/";item=$item}}
 Write-Output 'READY: three real local preview fixtures sent'
 while($true){
  $message=Read-Frame
  if($message.type -eq 'credentials'){
   if(!$message.url.StartsWith($base+'/')){Send-Frame @{type='error';requestId=$message.requestId;message='Not a test resource'};continue}
   Send-Frame @{type='credentialsResult';requestId=$message.requestId;url=$message.url;cookies=@();headers=@{}}
   Write-Output 'PASS preview requested credentials for its exact observed test resource'
  }elseif($message.type -eq 'scanStop') {Send-Frame @{type='scanState';scanning=$false}}
 }
}finally{
 if($process.Id){$process.StandardInput.Close();if(!$process.WaitForExit(3000)){$process.Kill($true)}}
 $process.Dispose()
}
