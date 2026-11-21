# Changelog

## Unreleased

0.11.1 did not start: its settings file turns on the navigation's
manoeuvres with a setting the program did not know, so it stopped at once
and the Store kept the version before. The program knows the setting now,
and a test loads the shipped settings the way the program does.

The turn card also shows roundabouts, with the exit to take, and forks
(keep left or right), and distances in kilometres. Two names the
navigation gives together, such as two towns a road leads to, have a dot
between them.

## 0.11.1

The turn card shows the navigation's next manoeuvre while it guides,
CarPlay's guidance included, instead of only the app's example: each turn
with its direction, and the destination with its side. Manoeuvres not
recognised yet (roundabouts, forks, ferries) show a plain mark with the
distance and the road. The card's lower position now sits below the
position arrow instead of over it. Its text is smooth and keeps accents,
and its arrows are drawn in BMW's style.

## 0.11.0

CarPlay's cluster map patch is applied in CarPlay's memory each time it
starts, and `/usr/bin/carplay` stays as it ships: no write of the system
partition to patch, remove or recover it. Updating from an earlier version
puts the original file back and keeps the map, with at most one head-unit
restart; where CarPlay already runs the earlier patch, none.

## 0.10.13

A turn card can show the next manoeuvre over the cluster map, below the
position arrow or at the top; it is off unless turned on in the app.

## 0.10.12

The diagnostic journal is kept in memory and written to storage only around
failures and at shutdown, which greatly reduces writes to the head unit's
storage.

