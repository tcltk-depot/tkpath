# Testcase for option -filloverstroke and gradient for -stroke and -fill
# ----------------------------------------------------------------
# Author : Vladimir Orlov
# Date   : 08/26/2026
# License: Tcl-License (aka BSD)
#
# Copyright © 2026 Vladimir Orlov
# ----------------------------------------------------------------

package require Tk
package require tkpath ; # version 0.4.4 with option -filloverstroke for all objects
#or
#load tkpath.dll Tkpath 
set t ".c_over"
toplevel $t
wm geometry $t 900x550
tkp::canvas $t.c -bg yellow
pack $t.c -fill both -expand 1 -padx 1m -pady 1m

$t.c gradient create linear -lineartransition [list 900 0 0 90] -stops {{0 red 1} {1 gray80 1}}
$t.c gradient create linear -lineartransition [list 90 0 0 90] -stops {{0 cyan 1} {1 chocolate 1}}

$t.c gradient create linear -lineartransition [list 40 40 300 300] -stops {{0 red 1} {0.5 gray80 1} {1 cyan 1}}
set myGradient [$t.c gradient create linear -stops {{0 "gray80"} {1 "blue"}}]
set rgr [$t.c gradient create radial -stops {{0 cyan 1} {0.5 gray80 1} {0.75 red 1} {1 snow 1}}]
$t.c create prect 80 240 280 340 -fill $rgr -stroke gradient1 -strokewidth 10 -rx 6

$t.c create pline 20 40 280 300 -strokewidth 20 -stroke gradient2
$t.c create ptext 100 80 -text "Stroke and Fill gradient" -fontsize 70 -fontweight bold -stroke gradient0 -strokewidth 15 -fill gradient1 -filloverstroke 1
$t.c create ptext 160 140 -text "filloverstroke=0" -fontsize 70 -fontweight bold -stroke red -strokewidth 8 -fill black -filloverstroke 0
$t.c create ptext 220 200 -text "filloverstroke=1" -fontsize 70 -fontweight bold -stroke red -strokewidth 8 -fill black -filloverstroke 1

lassign "40 200 40 200" x0 x1 xs xe
for {set i 1} {$i < 5} {incr i}  {
    set obj$i [$t.c create prect $xs 400 $xe 520 -stroke {} -strokewidth 20 -fill cyan -stroke red]
    $t.c create ptext $xs 400 -textanchor nw -text "-fill over stroke" -fontsize 20 -stroke black
    incr xs $x1
    incr xe $x1
}
$t.c itemconfigure $obj1 -stroke {}
$t.c itemconfigure [expr {$obj1 + 1}] -text "-stroke {}"
$t.c itemconfigure $obj2 -filloverstroke 0
$t.c itemconfigure [expr {$obj2 + 1}] -text "-filloverstroke 0\n-strokewidth 20"
$t.c itemconfigure $obj3 -filloverstroke 1
$t.c itemconfigure [expr {$obj3 + 1}] -text "-filloverstroke 1\n-strokewidth 20"
$t.c itemconfigure $obj4 -fillopacity 0.8 -strokeopacity 0.5
$t.c itemconfigure [expr {$obj4 + 1}] -text "-filloverstroke 0\n-strokewidth 20\n-fillopacity 0.8\n-strokeopacity 0.5"

