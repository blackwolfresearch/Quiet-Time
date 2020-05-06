# Quiet Time
 
At the risk of forever being known as one of the world's most evil parents, I am publishing this here because it might be helpful to someone. At the time of development, like all of you, I'm living through the COVID-19 pandemic. Everyone has a different situation, but mine involves taking care of a two year old and a four year old who are both as rambunctious as they are cute/adorable. Like their dad, they are also extremely stubborn. I've been doing this for over a month, mostly by myself while also trying to work. So... if anyone wants to pass judgement on me for creating this instrument of evil, that's my story.

## What It Does

This is an Arduino device that functions as a "Time Out Warden" so to speak. The thing is, you put a toddler in timeout (at least mine) and it's essentially a "screamfest" It's not like they sit there and "think about their behavior" quietly until the "Time Out" is over and then come out a newly reformed and perfectly behaved toddler. This device doesn't necessarily accomplish the latter either, but at least it helps with the former and provides a bit of "peace and quiet" for the highly stressed parent. 

The idea behind its operation is very simple. You don't get out of "Time Out" until you are quiet for a prescribed amount of time. By default, this is ten minutes. Ten minutes of being quiet is apparently quite challenging for a toddler, but your mileage may vary. Should a significant amount of noise be made (i.e., fussing and screaming), the device sounds an audible alarm, flashes a "sad face" on the 8x8 LED matrix display and increases the amount of time by another minute (not to exceed ten minutes). Upon final completion of the ten minutes of "Quiet Time," a melody plays and a "smiley face" appears on the display indicating that "Time Out" is now over.

This wasn't quite as simple to write as I thought it would be. I found out that the sound sensor baseline readings vary considerably depending on the environment AND the power source. Therefore, it was necessary to create a "calibration mode" where the device will listen to the sound in the room for one minute and establish a baseline for what "quiet" is. False triggers based on random background noise were also an issue.

## Parts Needed

The parts I used were:

- Arduino UNO board
- Piezo buzzer
- MAX 7219 8x8 LED matrix display
- Momentary contact pushbutton
- KY-038 sound sensor

## How To Use

Once the device powers up, it will show a "neutral face" on the LED matrix. The pushbutton can be used to put the device into one of two modes: timeout mode or calibration mode. These modes are selected by either a momentary "short press" of less than 1.5 seconds or a "long press" of greater than 1.5 seconds for calibration mode.

### Calibration Mode

To use the calibration mode, power up the device in the room it will be used in. Do the long press and wait for a minute until the cycle is complete. It is important that, during this time, the noise level of the room represents "quiet." The device will adjust its baseline for "quiet" based on the default level and a calculated offset from that, which is set to the loudest sound heard during the one minute calibration cycle.

### Time Out Mode

For "Time Out" mode, power up the device in the room after having calibrated it to the baseline level of noise. This is important because otherwise it may be too sensitive and that won't work out well in the end. When the device shows the "neutral face" on the display, just do a quick press of the pushbutton. The cycle will start.

During the cycle, the top seven rows of LEDs will represent the amount of time remaining. Sure, it could have been done with numbers, but this makes it very clear to even younger children. It eliminates confusion because if they misbehave, time will be added and it is much more obvious this way. When none of the LEDs remain lit, "Time Out" is over. A "smiley face" will appear and a melody will play.

The last row of LEDs is a bar graph that shows the detected noise level. When the bargraph is over the halfway point, this means the noise is over the threshold. To avoid false "time penalties, " the program will not issue a time penalty unless 20+ events happen over a period of 30 seconds, with at least 500ms in between each event. This is sufficient to prevent an unfair penalty if, for example, there is a cough or some brief noise outside.

It is important to know that the cycle cannot be canceled once started, unless the power is removed.

## Further Thoughts

When I developed this, I actually brought it to my kids and made sort of a "game" out of it. Strangely enough, they were curious and participated. It's so hard to get them to actually be quiet for any length of time, but this essentially "gamified" it and, as a result, worked surprisingly well. So maybe it has other uses and isn't so evil after all?

## Schematic

I will be getting a schematic of the device done soon. Right now, I have to get back to my actual work. Not sure if anyone will even be interested in this project, but I'll try to get it posted here reasonably soon.
