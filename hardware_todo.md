# Hardware Noise Reduction TODO

## 1. 100nF ceramic cap on mic VDD to GND
**Concepts: bypass decoupling, charge reservoir, supply impedance, PDM switching noise**

The mic's internal circuitry draws current in small bursts as it processes each PDM bit. Each burst causes a tiny voltage dip on the VDD pin because the supply line has inductance and resistance. The cap acts as a local charge reservoir — it supplies the burst current instantly, before the dip can propagate back up the supply line. Without it, every PDM clock edge causes a supply ripple that the mic's analog front end sees as noise on the signal.

---

## 2. Change oversampling back to 125
**Concepts: DFSDM oversampling ratio, Sinc filter decimation, sample rate, conducted emissions**

The PDM clock frequency = 64MHz / 32 = 2MHz. The mic is clocked at 2MHz regardless of oversampling — oversampling only controls how many PDM bits the Sinc filter accumulates per output sample. So changing oversampling from 125 to 50 didn't change the PDM clock, it just changed the output sample rate from 16kHz to 40kHz. No benefit for your use case, and the higher output rate means more UART traffic and more USB activity, both of which couple noise back into the system.

---

## 3. Low-noise LDO for mic VDD (e.g. LP2985, MIC5504)
**Concepts: power supply rejection ratio (PSRR), LDO noise spectral density, analog power isolation**

The Nucleo's onboard 3.3V regulator is a general purpose LDO designed for digital logic — it has relatively high output noise (tens to hundreds of µV). The mic's analog front end is extremely sensitive and amplifies that supply noise directly into the signal. A dedicated audio-grade LDO has output noise in the single-digit µV range. You power it from 5V USB and it gives the mic a clean, isolated 3.3V rail completely decoupled from whatever the Nucleo's digital logic is doing.

---

## 4. 33Ω series resistor on PDM clock line
**Concepts: RC low-pass filter, signal integrity, edge rate control, conducted and radiated EMI**

The PDM clock is a square wave — it has very fast rise and fall times (nanoseconds on a Cortex-M4 GPIO). Fast edges have high-frequency harmonic content that radiates as EMI and couples capacitively into adjacent traces and the power supply. The resistor forms an RC low-pass filter with the parasitic capacitance of the clock line, which slows the edges just enough to reduce the high-frequency content without affecting the clock's function. The mic only needs to see a clean logic transition, not a fast edge.

---

## 5. Twist or separate PDM clock and data lines
**Concepts: capacitive crosstalk, inductive crosstalk, common-mode rejection, transmission line theory**

Any wire carrying a signal acts as an antenna — it both radiates and picks up EMI. Twisting the clock and data wires together causes their electromagnetic fields to cancel each other (each twist reverses the field orientation). Shielding adds a grounded conductor around the wire that intercepts external EMI before it reaches the signal. On a short PCB trace this matters less, but with flying wires even a few centimetres can pick up significant interference from USB, switching regulators, and display circuitry nearby.

Currently the clock and data wires run parallel and adjacent — the worst possible arrangement. The clock is switching at 2MHz, creating a rapidly changing electric field. The parallel data wire acts as a receiving antenna for that field (capacitive crosstalk). There is also inductive crosstalk — the changing current in the clock line creates a changing magnetic field which induces a current in the data line loop. Twisting reverses which wire is on top each twist, so the crosstalk from each half-twist cancels the other. Practical alternatives if you can't twist: separate them by at least 1cm, or place a GND wire between them.

---

## 6. DFSDM Offset register
**Concepts: DC offset cancellation, PDM bit density bias, fixed-point arithmetic**

The DC offset comes from the DFSDM Sinc filter integrating a PDM bit stream that isn't perfectly 50/50 — the mic has a slight bias toward more 1s or 0s at rest. The DFSDM Offset register subtracts a fixed value from every sample in hardware before it reaches your code. Measure the average output at rest from the stream, then set:

```c
hdfsdm1_channel1.Init.Offset = -measured_dc_value;
```
