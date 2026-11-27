# Testcase for tkp::loupe.
# ----------------------------------------------------------------
# Author : Vladimir Orlov
# Date   : 08/26/2026
# License: Tcl-License (aka BSD)
#
# Copyright © 2026 Vladimir Orlov
# ----------------------------------------------------------------

package require Tk
package require tkpath ; # version 0.4.2 with function loupe
 
set t .c_loupe
toplevel $t -bg yellow
wm geometry $t +600+100
set img [image create photo -width 200 -height 200]
label $t.lab -image $img
#Screenshot of a rectangular screen region centered at 100x100, 200 pixels wide, and 200 pixels high.
::tkp::loupe $img 100 100 200 200
pack $t.lab -padx {5m 5m} -pady 5m -side left
set img1 [image create photo -width 200 -height 200]
label $t.lab1 -image $img1
#Screenshot of a rectangular screen region centered at 300x100, 400 pixels wide, and 300 pixels high.
::tkp::loupe $img1 300 100 200 200
pack $t.lab1 -padx 0 -pady 5m -side left
set img2 [image create photo -width 400 -height 400]
label $t.lab2 -image $img2
#Screenshot of a rectangular screen region centered at 100x100, 200 pixels wide, and 200 pixels high. Zoom 2x
::tkp::loupe $img2 0 0 400 400 2
pack $t.lab2 -padx 5m -pady 5m
