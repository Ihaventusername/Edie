# Edie
​A hyper-minimalist, scriptable binary surgery tool. Sub-10KB (Dynamic) powerhouse designed for resource-constrained Linux environments


Build

## 1. clone
``` bash
git clone https://github.com/Ihaventusername/Edie.git
cd ./Edie
```
## 2. build
#### if you want more stable:
``` bash
gcc edie.c -Os -o ei
```
#### if you want more slim(about 8.3KB):
```bash
# the command is for aarch64/x86_64
gcc -Os -s -static-libgcc -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -Wl,--gc-sections -Wl,-z,norelro edie.c -o ei
```

## HOW TO USE
1. File Handling
 * Open/Create: +{filename} (e.g., +{config.bin})
 * Save: ={filename} (e.g., ={backup.dat})
 * Quick Save: ={$} (Saves back to the currently opened file)
 * Exit: EXIT
2. Navigation
 * Move Right: > (Moving past the end of the file automatically pads it with spaces or nulls)
 * Move Left: <
 * Jump to Start: )
 * Jump to End: (
 * Inspect: P (Shows a hex dump of the bytes around the current pointer)
3. Editing Data
 * Insert Mode: ^^data^^ (Pushes existing data forward)
 * Overwrite Mode: %%data%% (Replaces data at the current pointer)
 * Hex Mode: Start the program with -H to input data as hex pairs (e.g., %%FF00AA%%)
4. Macro Loops
 * Syntax: |count instructions|
 * Example: |10 >| (Move 10 steps right)
 * Example: |1048576 ^^A^^| (Generate a 1MB file of the letter 'A')
5. Visual Mode (Radar View)
 * Enter/Exit: Press Ctrl+V (or type ^V in the command line)
 * Navigation: Use < and > to move the cursor while viewing data.
 * Quick Edit: Start typing any command (like %%new%%) while in Visual Mode. The command will appear in the brackets; press ENTER to execute it immediately without leaving the view.
 * Exit View: Press Ctrl+V again to return to the main console.
