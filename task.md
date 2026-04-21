during this task you will be fixing a big problem with blocked currently there is only one blocked queue 
there should be one for each type of blocked state

1. console read
2. console write
3. file read/write for each file

also currenly flags say blocked but it doesnt specify what type of blocked state it is
so we should add a new type of blocked state for each type of blocked state

there should be also an unblock flag for each resource 

the schdular should be updated accordingly it should at each tick check for the blocked and unblocked flags and update the location of process accordingly

lastly make sure to update the gui to reflect the changes