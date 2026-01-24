# Edie
​A hyper-minimalist text editor Sub-10KB (Dynamic) powerhouse designed for resource-constrained Linux environments


Build

## 1. Clone
``` bash
git clone https://github.com/Ihaventusername/Edie.git
cd ./Edie
```
## 2. Build
#### Normal build:
``` bash
make
```
#### Slim build (around 8.3KB):
```bash
make slim
```
#### Static build:
```bash
make static
```
#### Install:
``` bash
make install
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
 * Exit View: Press Esc
