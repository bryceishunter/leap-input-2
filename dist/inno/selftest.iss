; Checks the installer's helper code without installing anything. build-installers.ps1
; compiles this next to the configured leapdesk-kvm.iss, runs it with /LOG, and fails
; the build on any line starting with FAIL. Setup exits before showing a window.

#define SelfTest
#define OutputName "LeapdeskKVM-selftest"
#include "leapdesk-kvm.iss"

[Code]
var
  SelfTestFailures: Integer;

procedure Check(const Name: String; Passed: Boolean);
begin
  if Passed then
    Log('PASS ' + Name)
  else
  begin
    Log('FAIL ' + Name);
    SelfTestFailures := SelfTestFailures + 1;
  end;
end;

procedure CheckEqual(const Name, Actual, Expected: String);
begin
  Check(Name + ' (got "' + Actual + '")', Actual = Expected);
end;

<event('InitializeSetup')>
function SelfTestInitializeSetup(): Boolean;
var
  Client, Server, Legacy, Config: String;
  Args: TArrayOfString;
begin
  Client := '"C:\Program Files\Leapdesk\leapdesk-client.exe" -f --no-tray --ipc --debug NOTE ' +
            '--name surfacestudio --profile-dir "C:\Users\A B\AppData\Local\Leapdesk" ' +
            '--log "C:\Users\A B\AppData\Local\Leapdesk\client.log" 100.111.152.25:24800';
  Server := '"C:\Program Files\Leapdesk\leapdesk-server.exe" -f --no-tray --ipc --debug NOTE ' +
            '--name DESKTOP-GP522K1 --profile-dir "C:\Users\A B\AppData\Local\Leapdesk" ' +
            '-c "D:\dev\leapdesk\deploy\leapdesk.sgc"';
  Legacy := '"C:\Program Files\InputLeap\input-leapc.exe" -f -n studio 10.0.0.2';

  Args := SplitCommand(Client);
  Check('splits into 13 arguments', GetArrayLength(Args) = 13);
  CheckEqual('keeps a quoted path whole', Args[0], 'C:\Program Files\Leapdesk\leapdesk-client.exe');
  CheckEqual('keeps a quoted path with a space whole', Args[9], 'C:\Users\A B\AppData\Local\Leapdesk');

  CheckEqual('reads --name', OptionValue(Client, '-n', '--name'), 'surfacestudio');
  CheckEqual('reads -n', OptionValue(Legacy, '-n', '--name'), 'studio');
  CheckEqual('reads -c', OptionValue(Server, '-c', '--config'), 'D:\dev\leapdesk\deploy\leapdesk.sgc');
  CheckEqual('a missing option is empty', OptionValue(Client, '-c', '--config'), '');

  CheckEqual('a client command ends with the server', CommandServer(Client), '100.111.152.25:24800');
  CheckEqual('an Input Leap client command too', CommandServer(Legacy), '10.0.0.2');
  CheckEqual('a server command has no server', CommandServer(Server), '');

  Check('accepts a host name', IsValidScreenName('DESKTOP-GP522K1'));
  Check('accepts dots and underscores', IsValidScreenName('studio.local_2'));
  Check('rejects spaces', not IsValidScreenName('my pc'));
  Check('rejects an empty name', not IsValidScreenName(''));

  CheckEqual('picks the first non-empty value', FirstNonEmpty('', 'b', 'c'), 'b');
  CheckEqual('uses NOTE without /LOGLEVEL', LogLevel(), 'NOTE');
  CheckEqual('a service that does not exist', ServiceState('LeapdeskSelfTestNoSuchService'), 'MISSING');
  Log('Leapdesk service state: ' + ServiceState('{#ServiceName}'));

#if Role == "Server"
  Config := ExpandConstant('{tmp}\selftest.sgc');
  SaveStringToFile(Config, 'section: screens' + #13#10 + #9 + 'Desk-1:' + #13#10 + '    studio:' + #13#10 +
                   'end' + #13#10, False);
  Check('finds a screen in the configuration', ConfigHasScreen(Config, 'desk-1'));
  Check('finds an indented screen', ConfigHasScreen(Config, 'studio'));
  Check('misses a screen not in it', not ConfigHasScreen(Config, 'laptop'));
#endif

  Log(Format('Self-test finished with %d failure(s)', [SelfTestFailures]));
  Result := False;
end;
