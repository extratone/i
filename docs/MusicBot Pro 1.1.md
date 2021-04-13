# MusicBot Pro 1.1

(This is an excerpt from [Issue 221](https://mailchi.mp/macstories/ghuteogwhou5g5uowhwhgu5uwhgo5uwhgtpbhywtigb4t359l) of _MacStories Weekly_ for April 24th, 2020.)

Ever since its original release in December 2019, I've been working on and off on a fairly substantial update to MusicBot Pro, the Club-exclusive version of MusicBot. For those not familiar with MusicBot, here's how [I introduced it on MacStories](https://www.macstories.net/ios/introducing-musicbot-the-all-in-one-apple-music-assistant-powered-by-shortcuts/):

I created MusicBot for two reasons: I wanted to speed up common interactions with the Music app by using custom actions in the Shortcuts app; and I also wanted to build a series of "utilities" for Apple Music that could be bundled in a single, all-in-one shortcut instead of dozens of smaller, standalone ones.

The result is, by far, the most complex shortcut I've ever ever created (MusicBot spans 750+ actions in the Shortcuts app), but that's not the point. MusicBot matters to me because, as I've shared before, music [plays an essential role in my life](https://www.macstories.net/stories/i-made-you-a-mixtape/), and MusicBot lets me enjoy my music more. This is why I spent so much time working on MusicBot, and why I wanted to share it publicly with everyone for free: I genuinely believe MusicBot offers useful enhancements for the Apple Music experience on iOS and iPadOS, providing tools that can help you rediscover lost gems in your library or find your next music obsession.

And here's how I described MusicBot Pro in [Issue 205 of MacStories Weekly](https://mailchi.mp/macstories/gshuihgo8w37tghyo983ow3h5y8qhuoy3ghbkguqhiq9ovx15780dsvhfvuiwk47jukr6ur6kj):

Here's the gist of what you can find in MusicBot Pro, which you can download at the end of this section: MusicBot Pro removes several of MusicBot's limitations in terms of dealing with the Apple Music online catalog; while MusicBot can only process songs already saved to your library, MusicBot Pro can talk directly to the Apple Music API, which means you'll be able to search for any item (songs, artists, albums, playlists), love or dislike songs, save curated playlists, and more. Furthermore, MusicBot Pro comes with two exclusive Home screen icons, also designed by [Michael Flarup](https://twitter.com/flarup) exclusively for Club MacStories members.

As I also detailed in the post on MacStories earlier this week, MusicBot Pro requires the [Toolbox Pro app](https://apps.apple.com/us/app/toolbox-pro-for-shortcuts/id1476205977) to be installed with the Premium feature pack unlocked as a $5.99 In-App Purchase.

Today, I'm pleased to announce the release of MusicBot Pro 1.1 for Club MacStories members. Despite a series of lingering issues in the Shortcuts app, I've been able to add a variety of new Apple Music integrations and bug fixes, bringing the number of actions included in MusicBot Pro to over 1,300; it is, by far, the largest, most comprehensive shortcut I've ever created.

Before I dive into some of the details of this release, here's the full changelog of what's new and improved in MusicBot Pro 1.1:

  * Added support for saving links from the clipboard while running MusicBot inside Shortcuts. This lets you avoid using the share sheet if you want to share a song or album from Apple Music
  * Added support for the new Get Up! Mix
  * Added new options to 'Album from Current Song'
    * Popular Songs by Artist shows top songs by the current artist
    * The Album option is now based on the Apple Music API
  * Added option to pick individual songs from albums and playlists in Apple Music Search, with additional playback options
  * When searching for an artist in the Apple Music catalog, you can now pick multiple top songs from that artist from Apple Music
  * Fixed typo in title of Favorites menu
  * Revamped Current Artist or Song menu with emoji
  * Using fewer actions to open items directly in the Music app
  * Fixed missing Go Back actions throughout the shortcut
  * Added notifications to indicate status of Apple Music requests

As you can see, there are some nice additions in this version of MusicBot Pro. Allow me to highlight some of my favorites.

#### Save Links from the Clipboard

One of the most annoying bugs of the Shortcuts app is how, due to MusicBot Pro's sheer size, it often fails to pass a song or album from the Music app to the shortcut in the share sheet. If it doesn't fail, it just takes way too long to pass an item from Music to MusicBot Pro. To work around this problem, I've added a new option to MusicBot's 'Utilities & Queue' menu called 'Save Link from Clipboard'.

![Processing links from the clipboard in MusicBot Pro](https://2672686a4cf38e8c2458-2712e00ea34e3076747650c92426bbb5.ssl.cf1.rackcdn.com/2020-04-24-11-55-11.jpeg)

As the name implies, this action lets you process Apple Music links copied to the clipboard inside MusicBot itself, without using the share sheet in the Music app. If you have an Apple Music link in your clipboard, you can choose this action to save it as a favorite, add it to your collection of new releases, check out its release date, or generate shareable links for multiple streaming services. Until Apple fixes the share sheet performance in iOS 13 for large shortcuts such as MusicBot Pro, I recommend processing links this way.

#### Play the Get Up! Mix

Last month, Apple [introduced](https://www.macstories.net/linked/apple-music-debuts-new-get-up-mix-algorithmic-playlist/) a new smart playlist for Apple Music subscribers called the 'Get Up! Mix', which the company described as full of "happy-making, smile-finding, sing-alonging music". In MusicBot Pro 1.1, you'll be able to play the Get Up! Mix by choosing it as an option under the 'Apple Mixes' category.

![Playing the new Get Up! Mix in MusicBot Pro. ](https://2672686a4cf38e8c2458-2712e00ea34e3076747650c92426bbb5.ssl.cf1.rackcdn.com/2020-04-24-11-58-22.jpeg)

As with other Apple mixes supported by MusicBot, you can take advantage of multiple playback options: you can play the entire playlist in its original order, play it on shuffle, or pick individual songs from it and play them in the order you prefer.

#### Deeper Apple Music API Integration

Besides the list of aforementioned fixes and improvements, I've also extended MusicBot Pro's integration with Apple Music by including additional Toolbox Pro actions based on the Apple Music API. When searching for the current song's artist, for example, you'll now see a new action that presents you with a list of popular songs from that artist as fetched from the Apple Music catalog; or, when searching for albums or playlists in the Apple Music catalog, you can now pick individual songs and play them directly from MusicBot.

![New search and playback options based on the Apple Music API.](https://2672686a4cf38e8c2458-2712e00ea34e3076747650c92426bbb5.ssl.cf1.rackcdn.com/2020-04-24-12-02-54.jpeg)

As I noted above, MusicBot Pro requires [Toolbox Pro](https://apps.apple.com/us/app/toolbox-pro-for-shortcuts/id1476205977) since it takes advantage of rich menus and native Apple Music integrations; if you have the Toolbox Pro beta installed on your device and MusicBot Pro crashes the Shortcuts app, I recommend installing the App Store version of Toolbox Pro instead.

I've been working on this MusicBot Pro update for a long time, and I hope you'll enjoy it. As a final reminder, you can read my original description of MusicBot Pro and find a set of Club MacStories-exclusive custom Home screen icons for it in [Issue 205 of MacStories Weekly](https://mailchi.mp/macstories/gshuihgo8w37tghyo983ow3h5y8qhuoy3ghbkguqhiq9ovx15780dsvhfvuiwk47jukr6ur6kj).

You can [download MusicBot Pro 1.1 here](https://www.icloud.com/shortcuts/eb0298be57934ce7af643f95cba8c49f).