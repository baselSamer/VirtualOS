Types of system calls required:
1. Read the data of any file from the disk.
2. Write text output to a file in the disk.
3. Print data on the screen.
1
German University in Cairo
Faculty of Media Engineering andTechnology
Dr. Aya Salama
2
4. Take text input from the user.
5. Reading data from memory.
6. Writing data to memory.

Program Syntax
For the programs, the following syntax is used:
• print: to print the output on the screen. Example: print x
• assign: to initialize a new variable and assign a value to it. Example: assign
x y, where x is the variable and y is the value assigned. The value could be
an integer number, or a string. If y is input, it first prints to the screen "Please
enter a value", then the value is taken as an input from the user.
• writeFile: to write data to a file. Example: writeFile x y, where x is the
filename and y is the data.
• readFile: to read data from a file. Example: readFile x, where x is the
filename
• printFromTo: to print all numbers between 2 numbers. Example: printFromTo x
y, where x is the first number, and y is the second number.
German University in Cairo
Faculty of Media Engineering andTechnology
Dr. Aya Salama
4
• semWait: to acquire a resource. Example: semWait x, where x is the resource name. For more details refer to section Mutual Exclusion
• semSignal: to release a resource. Example: semSignal x, where x is the
resource name. For more details refer to section Mutual Exclusion
