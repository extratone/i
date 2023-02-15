# SMS Clipboard Shortcut
Updated `05152023-120535`

- [GitHub Issue](https://github.com/extratone/i/issues/299)
- [**RoutineHub Page**](https://routinehub.co/shortcut/14265/)
- [David Blue’s RoutineHub Library](drafts://open?uuid=CA94DF33-CAB9-40A0-836E-806225D5B600)
- [iCloud Share Link](https://www.icloud.com/shortcuts/03e4896705dd4fffb7b1d61556891b08)
- [Working Copy](working-copy://open?repo=i&path=shortcuts&mode=content)
- [Source Repo File](https://github.com/extratone/i/blob/main/shortcuts/SMSClipboard.shortcut)
- [WTF](https://davidblue.wtf/drafts/[[uuid]].html)
- [Things](things:///show?id=RwcdujJomagnsBio7xtofr)

---

## Social

- Telegram

---

## Send the contents of the Mac system clipboard to a single phone number.

**Please note**: you'll need to change the value of the phone number within the `Run Applescript` action with your own intended target.

This shortcut uses [Applescript](https://gist.github.com/fe653e065f5661adca563a54bee4622e) to send the value of the Mac system clipboard to the provided recipient using the Messages app.

The full script is below:

```applescript
tell application "Messages"		set targetBuddy to "+15738234380"	set targetService to id of 1st account whose service type = iMessage			set textMessage to (the clipboard)		set theBuddy to participant targetBuddy of account id targetService	send textMessage to theBuddy	end tell
```

## Contact

- [Contact Card](https://davidblue.wtf/db.vcf)
- [Telegram](https://t.me/extratone)
- [Email](mailto:davidblue@extratone.com) 
- [Twitter](https://twitter.com/NeoYokel)
- [Mastodon](https://mastodon.social/@DavidBlue)
- [Discord](https://discord.gg/0b9KQUKP858b0iZF)
- [*Everywhere*](https://raindrop.io/davidblue/social-directory-21059174)...