@echo off
SET version=0.0.1.0.1
SET devroot=.
rem SET type=stable

perl package.pl version "%devroot%" HueBridge %version% %type%
del "%devroot%\HueBridge*.zip"
7z.exe" a -r "%devroot%\HueBridge-%version%.zip" "%devroot%\plugin\*"
perl package.pl sha "%devroot%" HueBridge %version% %type%




