# Batch Shortcuts Signer (iOS) Shortcut

- [GitHub Issue](https://github.com/extratone/i/issues/170)
- [**RoutineHub Page**](https://routinehub.co/shortcut/11467)
- [iCloud Share Link](https://www.icloud.com/shortcuts/6d99f93cdac94d3d9b9cd928c1287cea)
- [Batch Shortcuts Signer-macOS Shortcut](drafts://open?uuid=5D4D9120-69EC-4449-AB7F-35C0D97BBA7D)
- [Batch Shortcuts Signer (iOS) Shortcut](https://davidblue.wtf/drafts/03AEAC14-EB74-44F0-8010-9844B15E5DE3.html)

## Social

- [Telegram](https://t.me/extratone/10868)

---

## Automate `shortcuts sign` (remotely!) to export shareable .shortcut files by folder.

*See [**the macOS version**](https://routinehub.co/shortcut/11401) of this shortcut to export signed .shortcut files locally.*

This is a modification of Federico Viticci’s “[Shortcut Injector,](https://www.icloud.com/shortcuts/fa780dd6de044d878c4c827009651a56),” about which you can read more details  in “[Creating, Modifying, and Signing Shortcuts on macOS](https://club.macstories.net/posts/creating-modifying-and-signing-shortcuts-on-macos).” It is identical to [the version I published for macOS](https://routinehub.co/shortcut/11401) with additional considerations for performing the signatures remotely over SSH. At install, you will be prompted to specify:
1. The address of the host Mac on which signatures will be performed.
2. The username of the account on said host Mac from which you will be signing.
3. A port (other than `22`) to use for the SSH connection.
4. The password of said user account on the host Mac.

You will also need to explicitly set two directories: one for the  pre-signed, post-export files saved only because they cannot otherwise be passed to the export tool (most will want to delete these, I assume,) and another to be the final destination of the signed output files. The first is just a [Folder](https://www.matthewcassinelli.com/actions/folder/) action set to `Ask Each Time` and the second is a simple [Text](https://www.matthewcassinelli.com/actions/text) action that represents a pointer originating from your home folder (~).

`shortcuts sign -m anyone -i "Pre-Signed Export Path" -o "Signed.shortcut"`

I should note that by default, the shortcut comes with a [Text Case](https://apps.apple.com/us/app/text-case/id1492174677) action designed to transform the input shortcut’s original filename into [camel case](https://en.m.wikipedia.org/wiki/Camel_case), mostly because I am 100% fed up with URL-encoded links’ tendency to illicit inexplicable and inconsistent behavior from iOS apps, at least.

That said, please note that said Text Case action is **100% optional**. Substitute a [Rename File](https://www.matthewcassinelli.com/actions/rename-file/) action if you’ d like.

After the [Repeat With Each](https://www.matthewcassinelli.com/actions/repeat-with-each/) cycle completes (and the files are signed,) an additional remote command is run:

`ls -1 -d "$PWD/Your specified directory/"*`

...and the result - an unformatted list of the *full file path* for every signed shortcut in the directory - is copied to the clipboard and finally shown via [Quick Look](https://www.matthewcassinelli.com/actions/quick-look/).

![Batch Shortcuts Signer-iOS](https://github.com/extratone/i/raw/main/shortcuts/BatchShortcutsSigner-iOS.PNG)

You can find the source files - including [an HTML overview](https://davidblue.wtf/shortcuts/batchshortcutssigner-ios.html) - in [the /shortcuts directory of my iOS GitHub Repository](https://github.com/extratone/i/tree/main/shortcuts).

[**Video Demo**](https://user-images.githubusercontent.com/43663476/161095955-062aacf9-4ac3-49fc-82a4-7a9f1e132cfd.MOV)

<video controls>
  <source src="https://user-images.githubusercontent.com/43663476/161095955-062aacf9-4ac3-49fc-82a4-7a9f1e132cfd.MOV">
</video>

For good measure, here’s the result of `shortcuts help sign`:

```
OVERVIEW: Sign a shortcut file.

You can use this command to sign a shortcut file. It also supports signing a shortcut in the old format.

USAGE: shortcuts sign [--mode <mode>] --input <input> --output <output>
shortcuts help --verbose
OPTIONS:
  -m, --mode <mode>       The signing mode. (default: people-who-know-me)
  -i, --input <input>     The shortcut file to sign. 
  -o, --output <output>   Output path for the signed shortcut file. 
  -h, --help              Show help information.
```

---

## Contact

- [Contact Card](https://davidblue.wtf/db.vcf)
- [Telegram](https://t.me/extratone)
- [Email](mailto:davidblue@extratone.com) 
- [Twitter](https://twitter.com/NeoYokel)
- [Mastodon](https://mastodon.social/@DavidBlue)
- [Discord](https://discord.gg/0b9KQUKP858b0iZF)
- [*Everywhere*](https://raindrop.io/davidblue/social-directory-21059174)...