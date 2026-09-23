Windows server and client installers, built with `dist/inno/build-installers.ps1`.
They stop and remove earlier Leapdesk and Input Leap services and installs,
install afresh, move an Input Leap pairing certificate and configuration over,
and set the Leapdesk service to restart after a crash. The server installer
includes the settings window.
