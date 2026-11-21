# Changelog

## Unreleased

The turn card shows the navigation's next manoeuvre while it guides,
CarPlay's guidance included, instead of only the app's example. Directions
not recognised yet show a plain mark with the distance and the road. The
card's lower position now sits below the position arrow instead of over it.

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

