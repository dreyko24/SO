@echo off
rem Puente web para Koenigreich OS: dejalo abierto mientras usas el navegador de vortex (host desde la VM: 10.0.2.2:8080)
python tools\vortex-proxy.py 8080 || py tools\vortex-proxy.py 8080
pause
