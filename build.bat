@echo off
set JAVA_HOME=E:\Android-SDK\jdk-17
set PATH=%JAVA_HOME%\bin;%PATH%
cd /d E:\Android-SDK
gradlew.bat %*
