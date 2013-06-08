REBOL []

#do [include-ctx/push [%../../r3-gui/release/]]

#include-check %r3-gui.r3

#do [include-ctx/pop]

dpi: gui-metric 'screen-dpi
gui-metric/set 'unit-size dpi / 96
scr: round (gui-metric 'work-size) - gui-metric 'title-size 

img: decode 'jpeg  #include-binary %clouds.jpg
print ""

view/options [
	vgroup [
		vpanel sky [
			image img
			vtight [
				title "Hello world!"
				text "This REBOL script has been encapped for Android. "
				button "Close" on-action [quit]
			] options [box-model: 'frame show-mode: 'fixed gob-offset: 10x10 max-hint: [150 auto]]
		] options [
			max-hint: [800 auto]
			border-size: [1x1 1x1]
			border-color: black
		]
	] options [
		max-hint: scr
		pane-align: 'center
	]
][
	offset: 0x0
	init-hint: scr
	max-hint: scr
] 
