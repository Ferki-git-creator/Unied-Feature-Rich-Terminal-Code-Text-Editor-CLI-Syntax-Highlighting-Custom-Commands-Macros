  _   _ _   _ ___ _____ _____ ____  
 | | | | \ | |_ _| ____| ____|  _ \ 
 | | | |  \| || ||  _| |  _| | | | |
 | |_| | |\  || || |___| |___| |_| |
  \___/|_| \_|___|_____|_____|____/ 
                                    
   U N I V E R S A L   E D I T O R
        
        
------------------------------------------------------------

== DESCRIPTION ==
Unied (short for "UNIfied EDitor") is a lightweight, intuitive
terminal editor built for developers who value speed,
customization, and the philosophy of "edit with language".

== FEATURES ==
 • Intelligent syntax highlighting (comments, strings, numbers).
 • Intuitive command mode (Command Puzzle System).
 • Custom macros for your workflow.
 • Undo/Redo change history.
 • Dynamic hints in the status bar.

== INSTALLATION ==

** Debian / Ubuntu **
    $ sudo apt update
    $ sudo apt install build-essential libncursesw5-dev

** Fedora / RHEL **
    $ sudo dnf install gcc ncurses-devel

== COMPILATION ==
    $ gcc main.c -o unied -lncursesw

== RUNNING ==
    $ ./unied                  (create a new file)
    $ ./unied my_code.c        (open an existing file)

------------------------------------------------------------
== HOTKEYS (Ctrl+...) ==

 Ctrl+S  → Save file
 Ctrl+O  → Open file
 Ctrl+Q  → Quit
 Ctrl+F  → Search
 Ctrl+Z  → Undo
 Ctrl+Y  → Redo
 Ctrl+C  → Copy (line or selection)
 Ctrl+X  → Cut
 Ctrl+P  → Paste
 Ctrl+A  → Select all
 Ctrl+G  → Go to line
 Ctrl+H  → Help
 Ctrl+W  → Jump to start of word
 Ctrl+R  → Jump to end of word
 Ctrl+E  → Jump to end of file

------------------------------------------------------------
== CURSOR NAVIGATION ==

 ↑ ↓ ← →        → Standard movement
 Home / End     → Line start / end
 PgUp / PgDn    → Scroll by pages

------------------------------------------------------------
== EDITING ==

 Typing text      → just type
 Enter            → new line
 Backspace / Del  → delete character to the left
 Delete (KEY_DC)  → delete character under cursor or join lines

------------------------------------------------------------
== VISUAL MODE (SELECTION) ==

 Ctrl+V     → toggle visual mode
 Ctrl+C     → copy selection
 Ctrl+X     → cut selection
 Delete     → delete selection
 ESC        → cancel selection

------------------------------------------------------------
== COMMAND MODE (Ctrl+\\) ==

 Press Ctrl+\\ → Enter Command Mode:

 Commands:
   S     → save
   SA    → save as
   F     → find
   FN    → find next
   FP    → find previous
   R     → find and replace
   LN    → enable line numbers
   UL    → upper line
   LL    → lower line
   DU    → duplicate line
   DL    → delete line
   QW    → quit without saving
   I     → file info
   R     → recent files
   KN    → standard mode (WASD)
   TC    → type: code
   CT    → type: text
   h j k l → move cursor like in Vim
   Z     → undo
   Y     → redo
   ?     → help

 TAB     → autocomplete
 ESC     → exit mode
 ENTER   → execute command

------------------------------------------------------------
== CREATING MACROS (CREATIVE MODE) ==

 1. Enter Command Mode → Ctrl+\\
 2. Enter your sequence (e.g., MYCMD)
 3. Press ::
 4. Enter action: upper, lower, duplicate, quit_confirm, save_file
 5. Call your macro like a regular command.

------------------------------------------------------------
== SUPPORT THE DEVELOPER ==

 If you appreciate this software, please consider a donation:

 [ https://ko-fi.com/ferki ]

------------------------------------------------------------
(c) 2025 UNIED Development Team. All rights reserved.
"Edit with language, not with keybindings!"
------------------------------------------------------------
