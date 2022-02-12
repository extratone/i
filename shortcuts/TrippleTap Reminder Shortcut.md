# TrippleTap Reminder Shortcut

- [**RoutineHub Page**](https://routinehub.co/shortcut/11038)
- [iCloud Share Link](https://www.icloud.com/shortcuts/9e9ef38789ab418ca11371968a538611)
- [GitHub Issue](https://github.com/extratone/i/issues/138)
- [ShowCuts](https://showcuts.app/share/view/0bfc649d35384166bb2dce55f3cc9d7f)

## Video
- [Raw Link](https://user-images.githubusercontent.com/43663476/153483495-7b70a36b-8755-4911-bc0f-92c5323dfc46.MOV)
- [Tweet](https://twitter.com/NeoYokel/status/1491859495366057990)
- [Telegram](https://t.me/extratone/10123)

<video controls>
  <source src="https://user-images.githubusercontent.com/43663476/153483495-7b70a36b-8755-4911-bc0f-92c5323dfc46.MOV">
</video>
---

## Create a reminder with URL and last image taken.

![TrippleTap Reminder](https://user-images.githubusercontent.com/43663476/153335868-460caecf-a013-41e2-a448-862171dbb086.png)

This shortcut creates a reminder on a list set at install at a time in the future specified at install including a timestamp in [DavodTime](https://github.com/extratone/bilge/wiki/DavodTime), a URL captured from the clipboard, and the last image saved. I have it set to be triggered on the Triple tap [Back Tap](https://support.apple.com/en-us/HT211781) setting.

### Steps
1. An [Ask For Input](https://www.matthewcassinelli.com/actions/ask-for-input/) action prompts for the reminder title.
2. A duration of your choosing is added to the current time with an [Adjust Date](https://www.matthewcassinelli.com/actions/adjust-date/) action.
3. A [Get Latest Photos](https://www.matthewcassinelli.com/actions/get-latest-photos) action gets the latest photo saved.
4. A URL - if any - is grabbed from the clipboard.
5. A reminder is created using an [Add New Reminder](https://www.matthewcassinelli.com/actions/add-new-reminder) action with the details gathered above.