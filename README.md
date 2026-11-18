# `wiktionary-text-to-ipa`

`wiktionary-text-to-ipa` is a program that fetches IPA pronunciations for English words from Wiktionary.

## Compiling

It can be be compiled on Ubuntu by running

```
sudo apt install build-essential qt6-base-dev
```

to install GCC, `make` and Qt6 development files, and then running

```
qmake6
make
```

## Running

The program can be run by invoking

```
./wiktionary-text-to-ipa <word>
```

where `<word>` is the word you want the pronunciations to be fetched for. The output is a JSON document represented in one line. For example, invocation of

```
./wiktionary-text-to-ipa peace
```

results in

```
{"generalAmerican":"ˈpis","receivedPronunciation":"ˈpiːs","word":"peace"}
```

It is possible to fetch links to sounds with the pronunciations using `--with-sounds` option. With this option the example above is

```
./wiktionary-text-to-ipa --with-sounds peace
```

and it results in the output

```
{"generalAmerican":"ˈpis","generalAmericanSoundLicense":"https://commons.wikimedia.org/wiki/File:En-us-peace.ogg","generalAmericanSoundUrl":"https://upload.wikimedia.org/wikipedia/commons/6/61/En-us-peace.ogg","receivedPronunciation":"ˈpiːs","receivedPronunciationSoundLicense":"https://commons.wikimedia.org/wiki/File:En-uk-peace.ogg","receivedPronunciationSoundUrl":"https://upload.wikimedia.org/wikipedia/commons/e/e1/En-uk-peace.ogg","word":"peace"}
```

Here `<pronunciation>SoundUrl` is the URL of the sound with the IPA pronunciation and `<pronunciation>SoundLicense` is the link to the web page that contains the license for this sound file.

The description of these options can be seen by invoking the program with `--help` flag.

## Playing sounds with `jq`, `curl`, and `VLC`

It is possible to play sounds by extracting their URLs using `jq` program, then fetching them using `curl` program and playing using `vlc` program. The dependencies can be installed on Ubuntu by running the command

```
apt install jq curl vlc
```

and then invoking

```
curl $(./wiktionary-text-to-ipa --with-sounds peace | jq --raw-output '.generalAmericanSoundUrl') | vlc - vlc://quit
```

to play the sound for General American pronunciation and

```
curl $(./wiktionary-text-to-ipa --with-sounds peace | jq --raw-output '.generalAmericanSoundUrl') | vlc - vlc://quit
```

to play the sound for Received Pronunciation.

If `curl` does not downlod the sound files, try to use SOCKS5 proxy by adding the flag `--proxy=socks5://<proxy host>:<proxy port>` to `curl` command line arguments. Such a proxy can be made by connecting to a SSH server with the command

```
ssh <hostname> -D 9999 -N
```

then `<proxy host>` is `localhost` and `<proxy port>` is 9999.

Note that there are rate limits for downloading sounds, it is safe to download no more than one sound file per minute.
