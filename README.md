# Joybus Analyzers for Saleae Logic2

Saleae Logic2 analyzers for decoding the Joybus protocol used by Nintendo 64 and GameCube consoles and controllers.

![HLA Screenshot](img/hla.png)
![Data Table Screenshot](img/data-table.png)

## Low-Level Analyzer

The Joybus Low-Level Analyzer decodes the raw signal from the Joybus protocol into "byte" frames and "stop" frames.

Anything it cannot read becomes an "error" frame instead. Errors are drawn as red bubbles with a red X on the waveform, so a bad capture is visible without reading any text. A byte the decoder had to guess at keeps its value and is marked too, in red when a bit was held low for longer than any symbol and in amber when a bit sat between two symbol widths.

An unreadable run is only reported once the line has gone quiet and come back afterwards. A capture that opens or closes part way through a transmission holds an incomplete one through no fault of the bus, so neither end is reported.

## High-Level Analyzer

The Joybus High-Level Analyzer decodes the frames produced by the Low-Level Analyzer into human-readable transactions. The Low-Level Analyzer must be installed and active for the High-Level Analyzer to function.

It reports bad states as "error" frames of its own. These cover a command that went unanswered, a reply to a command that should not get one, a reply that ended before the command's fields were complete, and anything the Low-Level Analyzer could not read. A command is only known to have gone unanswered once something else appears on the bus, so the last command in a capture is never reported.

## Building the Low-Level Analyzer

```sh
cmake -B build && cmake --build build
```

Full details on building Saleae Low Level Analyzers can be found in the [SampleAnalyzer README](https://github.com/saleae/SampleAnalyzer).
