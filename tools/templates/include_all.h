[% INCLUDE $licence %]
/*
 *  * This code has been autogenerated, do not edit
 *   */

#ifndef [% module %][% name %]Msgs_H
#define [% module %][% name %]Msgs_H

[% IF install %]
[% FOREACH msg = msgs %]
#include <[% lib FILTER lower %]/[% module FILTER lower %]/[% module %][% name %]Msg[% msg.cls_name %].h>
[%- END %]
[% ELSE %]
#include "[% module %][% name %]Msg[% msg.cls_name %].h"
[% END %]

#endif