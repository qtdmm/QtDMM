# Web view: QtDMM in a browser

The web view shows the windows of QtDMM in a browser: the devices' panels,
graphs, plots and nodes, as they are on the screen, new every second. It is
only to look at: nobody can operate QtDMM through it. A second screen at the
bench, a colleague at their desk, a tablet or a kiosk screen in the lab can
watch a measurement that runs on another computer, a Raspberry Pi for
example.

## Switching it on

Under **Settings → Web view**:

| Setting | Meaning |
|---|---|
| *Enable the web view* | QtDMM answers browsers on a port of its own |
| *Port* | 8088 at first; when it is taken (another QtDMM), the next free one, the link names it |
| *Listen on* | *This computer only (localhost)* or *All network interfaces*, for other computers |
| *New picture every* | how often the browser asks for a new picture, 0.2 to 10 s (1 s at first) |

The section *Link* shows the link to open, with *Copy link*.

The picture is the window area: what lies in it, as large as it is, without
the sidebar and the toolbar. It is made only while a browser asks for it,
once per interval for all of them; with nobody watching the web view costs
nothing.

## The link

The link looks like `http://192.168.1.20:8088/3f9c…e1/`: the computer's
address in the network, the port and a secret of 32 characters. Without
the secret QtDMM answers *This link no longer works*.

The secret stays the same when QtDMM or the computer starts again. Keep the
link as a bookmark, write it on the kiosk screen's start page, give it to a
colleague. **Whoever has the link can watch.** *New link* in the settings
makes a new secret: every link given out until then stops working at once,
and a browser that shows it says so.

Where to get the link:

- **Sidebar:** *Services → Web view* - a double click opens it in the
  browser, the right-click menu has *Open in the browser*, *Show QR code...*
  (for a phone or tablet) and *Copy link*. The dot is green while a browser
  watches; the tooltip says how many.
- **Settings → Web view**, section *Link*.
- **Console:** QtDMM writes `Web view: http://…` when it starts the web view.
- **Over SCPI**, for a computer without a screen (over SSH, from a script or
  the qtdmm-mcp tool): `QTDMM:WEB:URL?` answers the link, `""` while the web
  view is off. See [SCPI server](scpi-server.md).

```sh
printf 'QTDMM:WEB:URL?\n' | nc raspi 5025
```

## The page

A narrow line at the top names the computer and the open project and the
time of the picture; the time turns red when no new picture came for three
intervals (QtDMM closed, the network gone). An alarm raised shows in the
line too. *Full screen* gives the whole screen to the picture; Esc leaves
it. The picture keeps its proportions and fills what is left. A tab in the
background asks for nothing.

## Safety

The web view uses plain HTTP, without encryption: anyone who can read the
network traffic can see the link and the pictures. Open it to the network
only where you trust it, and not to the internet. It takes no input: a
browser can only fetch the page, the picture and the line above it.
