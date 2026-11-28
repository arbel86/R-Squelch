R-Squelch by RBell — macOS
==========================

EASIEST: use the installer (R-Squelch-x.x.x-Mac.pkg).

The first time you open it, macOS may say it "cannot be opened because it is from an
unidentified developer" (R-Squelch is free and not signed with a paid Apple account):
  1. Right-click (or Control-click) the .pkg  ->  Open  ->  Open.
  2. If there is no Open button: System Settings -> Privacy & Security -> scroll down ->
     "R-Squelch… was blocked"  ->  Open Anyway.

MANUAL INSTALL (this zip):
  R-Squelch.vst3       ->  /Library/Audio/Plug-Ins/VST3
  R-Squelch.component  ->  /Library/Audio/Plug-Ins/Components
  R-Squelch.app        ->  /Applications
Then remove the download "quarantine" flag (Terminal):
  sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/R-Squelch.vst3 /Library/Audio/Plug-Ins/Components/R-Squelch.component /Applications/R-Squelch.app

Then rescan plug-ins in your DAW (in Logic: restart Logic; it checks the Audio Unit once).

Instagram: @rbell.music   SoundCloud: soundcloud.com/rbellmusic
