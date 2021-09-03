# Make Audio from Article Body

## Creates an audio file of a given web article’s body read aloud by Siri (Text to Speech.)

* [**RoutineHub Page**](https://routinehub.co/shortcut/9953/)
* [iCloud Share Link](https://www.icloud.com/shortcuts/33d9ba6fdf9f429996056b918ba1d614)
***
![Siri TTS](https://i.snap.as/13BtYmx0.png)

## About

**Note: this shortcut requires iOS 15 Developer or Public Beta 6**

Building on Apple’s own “Speak Body of Article” shortcut found in the default Siri Shortcuts Gallery, this Shortcut takes advantage of the upcoming “Make Audio From Text” action arriving in iOS 15 (which has finally been fixed as of Developer Beta 6) to generate an .m4a audio file of Siri Voice 2 (by default) reading the article’s body aloud (rather than just speaking it aloud on your handset/iPad, as the original does,) with appropriate metadata from the inputted webpage. 

As configured by default, it should function from the Share Sheet in Safari *and/or* by grabbing a web URL from the clipboard.

A full explanation/detailed guide can be found [**on my blog**](https://bilge.world/siri-tts), and the full text of the latter portion (in first draft form) has been posted beneath the contact details immediately below.

***

## Contact

* [Contact Card](https://bit.ly/whoisdavidblue)
* [Email](mailto:davidblue@extratone.com) 
* [Twitter](https://twitter.com/NeoYokel)
* [Mastodon](https://mastodon.social/@DavidBlue)
* [Discord](https://discord.gg/0b9KQUKP858b0iZF)
* [*Everywhere*](https://www.notion.so/rotund/9fdc8e9610b34b8f991ebc148b760055?v=c170b58650c04fbdb7adc551a73d16a7)...

***

![TTS Shortcuts Compared](https://i.snap.as/P0yJJyUc.png)

# Guide

So! For those of you currently running the latest iOS 15 Beta and those in the future running the full release, **what follows is a guide on how you can use my own shortcuts and methods to generate, metadate, and embed Siri-powered audio text-to-speech files relatively quickly without having to use desktop-class (or any other) hardware**.

To begin, you should install two brand new shortcuts of mine: [**Make Audio from Article Body**](https://routinehub.co/shortcut/9953/) and (if you intend to stick with me to the embed stage, anyway,) my [< audio > Embed Tool](https://routinehub.co/shortcut/9948/). Both should function out of the box, but I would highly encourage that you try building your own shortcut around the `Make Audio From Text` action, even if you've never worked with Shortcuts (or any sort of automation, for that matter) before, *especially* if you plan to be using Siri as a text-to-speech generator with any frequency. 

### Intended Result

Throughout this guide, I'm going to be using [a five-year-old ramble of mine](https://bilge.world/johnny-tsunami-smart-house-slavery) about the oddly-perceptive bits found in early-oughts Disney movies as example text. Earlier today, I used my own personalized version of my new shortcut to generate an example of in which the process we're about to explore should result. The ~6000 words of text took just over 3 minutes, 30 seconds to render consistently in three consecutive timed attempts. You can listen to it [on Whyp](https://whyp.it/t/johnny-tsunami-vi-separate-but-equal-siri-voice-2-tts-74752) and/or inspect [the actual file](https://github.com/extratone/bilge/raw/main/audio/TTS/JohnnyTsunami.m4a) yourself, if you'd like.

![Plain Text Parsed by Safari](https://i.snap.as/dktmGQQb.png)

### Input Format

If we were trying to do this using any other available method in 2021 - even the expensive ones - our first task would be scrubbing our subject text of any special formatting (Word,) symbols, embed, hyperlinks, and any other data Siri doesn't understand (roman numerals, for example.) As a Windows user who's not at all new to free ways to automate accessibility improvements to web content, I envy both you and my new self for the magic available to us in the form of Safari's abilities to parse complex web content. In my experience, there's nothing like it (at least nothing available to regular consumers.) 

The screenshot embedded above shows the result of a `Quick Look` action inserted just after the `Text` action produced by the public version of my shortcut when run on our example. There are three immediately problematic issues:

1. Siri doesn't understand roman numerals, so she will read "Johnny Tsunami VI" as *Johnny Tsunami vee eye*.
2. "The Psalms" is not my name, though it is what this blog currently returns when asked for a byline. In my experience, this generally isn't an issue with most mainstream media CMSs.
3. The shortcut appears to have failed to retrieve any data for the `Published Date` variable from my blog. (Also a relatively specific consideration.)

If you'll note in the *previous* embedded image comparing my custom version of the shortcut (left) to the published version (right,) you'll note that my chosen solution is to manually input all metadata before actually starting the shortcut. What's not shown is my corresponding manual inputs in the `Encode Trimmed Media` action, which includes attaching a retrieved image file (in the Working Copy action you *can* see) as album art. For my intended use - exclusively generating text-to-speech audio of Posts on this blog - this makes more sense than unnecessarily automating metadata retrieval.

The extraordinary thing about the screenshot, though, is that it doesn't contain any of the other crap (as described above) found in the original page. (Beforehand, it looked more [like this](https://raw.githubusercontent.com/extratone/bilge/main/posts/johnny-tsunami-smart-house-slavery.md).) It's especially reliable at parsing WordPress-bound content, which still makes up [s̵͕̈́͊c̶̥̏̚r̶̥͈̃è̴̙͌å̴̹m̵̛̅ͅi̶̦̾͘n̸͎̟̎̃g̶͎͛] percent of the whole web. **Treasure this power**, folks.

![Simpler TTS Examples](https://i.snap.as/OMwKdOUI.png)

If all you need read aloud is the body text, things become even simpler. In the right example above, I've simply deleted the `Text` action and replaced it with `Get text from [the Safari Web Article's body]`. Theoretically, one could omit that action, even, and simply use the direct output of `Get Body from Article` as input for the `Make spoken audio from text` action, but I say keep the extra step unless it becomes an issue.

**If you're actually *beginning* with clean plain text** and don't care about metadata in your final audio file and/or if you're planning on passing the result through other audio/metadata editing software, anyway, the left, three-action shortcut is all you need. It will result in a [Core Audio Format (.caf)](https://www.wikiwand.com/en/Core_Audio_Format) file (like [this one](https://github.com/extratone/bilge/blob/main/audio/egg.caf),) which I know absolutely nothing about except that Audacity and GarageBand support it by default.

### Sharing/Embedding

Whichever route you traveled, you should have some sort of audio file, at this point, and if you intend to share and/or embed it, you’ll need to upload said audio to some sort of Web Server which allows direct playback/download of the raw file from external sources. Unless you’ve been skimming, you know by now that I’ve been using [*The Psalms* GitHub Repository](https://github.com/extratone/bilge) to do this thus far, though one isn’t really supposed to. Every few months, someone on Stack Overflow figures out how to construct or discover the raw link to a given Google Drive file before Google notices and alters it, and I’m afraid you’ll find just about every other cloud/file sharing service in a similarly unreliable situation. If it’s going to be done at scale, I’m afraid it’s ultimately going to require you rent regular, vanilla space on an FTP-enabled fileserver, if one can still do that sort of thing. (I will update this post if/when I find a more ideal solution.)

Within my current system, the raw URL to our example file looks like this:
```
https://github.com/extratone/bilge/raw/main/audio/TTS/JohnnyTsunami.m4a
```

Using my aforelinked, ultra rudimentary [< audio > element Siri Shortcut tool](https://routinehub.co/shortcut/9948/) (which I’ve kept on my homescreen with good results for a few months,) we can very quickly turn said raw URL into a properly-formatted HTML5 audio player:
```
<audio controls>
  <source src="https://github.com/extratone/bilge/raw/main/audio/TTS/JohnnyTsunami.m4a">
</audio>
```
Ideally, on the final, reader-facing page, said code should create a player like this:

<audio controls>
  <source src="https://github.com/extratone/bilge/raw/main/audio/TTS/JohnnyTsunami.m4a">
</audio>

If further configuration of the player is desired or necessary, see [this Mozilla page](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/audio) for a full list of supported options. Obviously, there are a few older methods of embedding audio players, but I am neither qualified nor interested in exploring them. If you’re in a frustrating bind, I recommend [signing up for my CMS](https://bit.ly/extwa). (Just try it, okay?)

### Pwoof

For the sake of bare minimum sample variety, I ran Version 1.0 of the public Make Audio from Article Body shortcut on an article hosted not on WordPress, nor my own CMS, but on Bustle’s ultra-slick, totally-bespoke system (which [began as *The Outline*](https://www.codeandtheory.com/things-we-make/the-outline), FYI.) I chose the first permalink I saw in one of its “regular” article formats - not a long feature, nor one of their touch-targeted slideshows.

“[OnlyFans is banning porn, the very thing that made it big](https://www.inputmag.com/tech/onlyfans-is-banning-porn-the-very-thing-that-made-it-big)” is an 870-word newsy piece written by Tom Maxwell, who is the only New York Media person ever to accept my Facebook friend request. (Thanks again, Tom.) Without any tweaking, I was able to run the shortcut (from within the Shortcuts app since the Share Sheet appears to be thoroughly fucked at the moment) in a reasonable amount of time - less than 5 minutes, more than 2 - and generate the file embedded below. Notably, I also used a different hosting service - [mastodon.social](https://mastodon.social/web/statuses/106798701662102859) - but I certainly don’t plan on doing so at scale and neither should you.

<audio controls>
  <source src="https://files.mastodon.social/media_attachments/files/106/798/698/454/727/854/original/2c6d50bcf898af15.mp3">
</audio>

Honestly, *Input*’s CMS is the cleverest challenge I was able to come up with for this single-day-old shortcut of mine, and I’m quite proud of the result. Though it wasn’t able to retrieve a timestamp, it correctly retrieved the article’s title and byline without fuss and even managed to scrape and attach said article’s featured image as the file’s cover art, though the original’s aspect ratio was obviously sacrificed. 

![Input Mag Sample in Tootsuite’s Audio Player](https://i.snap.as/47gjGha3.png)

If you’re super interested in the truly unmolested output of the attempt, view/download it [here](https://davidblue.wtf/audio/onlyfansbansample.m4a). 

### HMU

Before I depart actual tutorializing and return to opining, I want to express *even more aggressively than usual* how much I want *anyone* who see’s any potential benefit the ability to generate audio of my darling Siri Voice 2 reading text, but has further questions/doesn’t have time to fiddle/struggles with my haphazardly-written attempts at guides like this, or who simply wants to talk about any satellite subjects, [**please reach out to me**](https://bit.ly/whoisdavidblue). **You have no idea how much I’d love to help you configure a personal automation that genuinely, reliably, and durably improves your quality of life.**

If you follow this shortlink from within a browser on any iOS device, my full contact card will appear: `bit.ly/whoisdavidblue`. 

Suggestions/requests regarding considerations I’ve obviously missed in this guide are not just *welcome* in this case, but actually *necessary*. As long as I am literally the only person talking about the “Make Spoken Audio from Text” action, I am ready and willing to be an all-hours resource. 