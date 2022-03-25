# Batch Shortcuts Signer (macOS) Shortcut

- [GitHub Issue](https://github.com/extratone/i/issues/165)
- [**RoutineHub Page**](https://routinehub.co/shortcut/11401)
- [iCloud Share Link](https://www.icloud.com/shortcuts/3536a2d8b3c44d509645da8dcccb16b7)

## Social

---

## Automate `shortcuts sign` to export shareable .shortcut files by folder.

![Batch Shortcuts Signer](https://davidblue.wtf/shortcuts/shortcutssigner.png)

This is a modification of Federico Viticci’s “[Shortcut Injector,](https://www.icloud.com/shortcuts/fa780dd6de044d878c4c827009651a56),” about which you can read more details  in “[Creating, Modifying, and Signing Shortcuts on macOS](https://club.macstories.net/posts/creating-modifying-and-signing-shortcuts-on-macos).” While I very much appreciate the craft Shortcut Injector encourages, I definitely *do not* have the time to move through three whole menu selections for each of the ~1600 shortcuts in my library (not that they’re all worth sharing.)

You *will* need to explicitly set two directories: one for the  pre-signed, post-export files saved only because they cannot otherwise be passed to the export tool (most will want to delete these, I assume,) and another to be the final destination of the signed output files. The first is just a [Folder](https://www.matthewcassinelli.com/actions/folder/) action set to `Ask Each Time` and the second is a simple [Text](https://www.matthewcassinelli.com/actions/text) action that represents a pointer originating from your home folder (~).

`shortcuts sign -m anyone -i "Pre-Signed Export Path" -o "Signed.shortcut"`

I should note that by default, the shortcut comes with a [Text Case](https://apps.apple.com/us/app/text-case/id1492174677) action designed to transform the input shortcut’s original filename into [camel case](https://en.m.wikipedia.org/wiki/Camel_case), mostly because I am 100% fed up with URL-encoded links’ tendency to illicit inexplicable and inconsistent behavior from iOS apps, at least.  

That said, please note that said Text Case action is **100% optional**. Substitute a [Rename File](https://www.matthewcassinelli.com/actions/rename-file/) action if you’ d like.

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

## References

- [Creating, Modifying, and Signing Shortcuts on macOS](https://club.macstories.net/posts/creating-modifying-and-signing-shortcuts-on-macos) | *MacStories*
- [Shortcuts Code Injection and 10 Innovations Apple Should Adopt from Third-Party Apps](https://www.macstories.net/linked/appstories-episode-261-shortcuts-code-injection-and-10-innovations-apple-should-adopt-from-third-party-apps/) | *AppStories* Episode 261

---

## Contact

- [Contact Card](https://davidblue.wtf/db.vcf)
- [Telegram](https://t.me/extratone)
- [Email](mailto:davidblue@extratone.com) 
- [Twitter](https://twitter.com/NeoYokel)
- [Mastodon](https://mastodon.social/@DavidBlue)
- [Discord](https://discord.gg/0b9KQUKP858b0iZF)
- [*Everywhere*](https://raindrop.io/davidblue/social-directory-21059174)...