; Copyright (c) 2026 The QtDMM developers
; SPDX-License-Identifier: GPL-3.0-or-later
;
; Included into the installer by CPack (CPACK_INNOSETUP_EXTRA_SCRIPTS).
; QtDMM's update (core/updatecheck.cpp, installerArguments()) runs the
; installer silently with /QTDMMRESTART=1: then QtDMM is started again
; when it is done, as the user who started the update, not as the
; administrator. A silent installation without it starts nothing.

[Run]
Filename: "{app}\bin\qtdmm.exe"; Flags: nowait runasoriginaluser; Check: QtDMMRestart

[Code]
function QtDMMRestart: Boolean;
begin
  Result := ExpandConstant('{param:QTDMMRESTART|0}') = '1';
end;
