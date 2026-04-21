now this is a more complec task I want you to implement a gui for the project when the system starts it should ask the user if he want to start in console or gui mode:
terminal mode is same as it is now
GUI:
    1. in the gui in the start we should be able to set the same info we sat in the console mode however everything should be dynamic and easy to use and understand
    2. after we start we will have two things the console and the gui running side by side:
        - the gui should show multi tude of things about the system:
            1. memory colored according to what process this memory is allocated to, hovering over a cell should show what type of memeory is it, if the cell does not belong to any show it as a grey cell
            2. show swap mem again as above
            3. show the current running pcb, the que info, incase of hern the current ratios and so on
            4. all those should be tabs so we can switch between them
            5. this gui gets updated after each teck a button should also be avilable to step through the ticks manually
        - terminal should remain active as the process them self and logs will be written to it