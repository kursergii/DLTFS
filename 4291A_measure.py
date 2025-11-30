import time
import pyvisa

# --- User settings ---
RESOURCE = "GPIB0::17::INSTR"   # change to your instrument resource
FREQS = [100e6, 200e6]          # frequencies to measure (Hz)
OUTPUT_FILE = "hp4291a_Zphi.csv"
TIMEOUT_MS = 20000              # instrument timeout

# --- Connect ---
rm = pyvisa.ResourceManager()
inst = rm.open_resource(RESOURCE)
inst.timeout = TIMEOUT_MS
inst.write_termination = "\n"
inst.read_termination = "\n"

# Identify
print("IDN:", inst.query("*IDN?"))

# Put instrument in single measurement mode and set display trace to Z,PHI if desired
# Note: HP/Agilent 4291A uses non-SCPI but similar commands; common commands below.
# Set analyzer to frequency list single-point measurement
inst.write("SING")        # single measurement trigger mode
inst.write("FUNC 'Z'")    # set measurement function to impedance magnitude (may be 'Z' or 'IMP')
time.sleep(0.2)

# Prepare output file
with open(OUTPUT_FILE, "w") as f:
    f.write("frequency_hz,impedance_ohm,phase_deg\n")

    for f_hz in FREQS:
        # Set frequency
        # For 4291A: use "FREQ <freq>" or "SENS:FREQ <freq>" depending on firmware.
        # Try standard SCPI-style first:
        try:
            inst.write(f"FREQ {f_hz}")
        except Exception:
            # fallback: use Hz suffix
            inst.write(f"FREQ {f_hz}HZ")
        time.sleep(0.1)

        # Trigger a single measurement and wait for completion
        # Use *TRG or ENAB/INIT depending on model; try *TRG then query OPC
        try:
            inst.write("*TRG")
            inst.query("*OPC?")
        except Exception:
            # fallback: use "SING" then local delay
            inst.write("SING")
            time.sleep(0.5)

        # Read impedance and phase. Common queries:
        #   READ?  -> returns measured values depending on setup
        #   OUTP? or CALC:DATA?
        # We'll try READ? and parse.
        try:
            resp = inst.query("READ?")
        except Exception:
            # some 4291A use 'OUTP?' or 'TRAC:DATA?'
            try:
                resp = inst.query("OUTP?")
            except Exception:
                resp = inst.query("TRAC:DATA?")

        # resp typically: "<Z>,<phase>,<other...>" or a list of values. Try to parse numbers.
        vals = [s for s in resp.replace(",", " ").split() if s.strip()]
        # Find first two numeric tokens
        nums = []
        for tok in vals:
            try:
                nums.append(float(tok))
            except ValueError:
                # strip non-numeric trailing chars like "OHM" or "DEG"
                tok2 = "".join(ch for ch in tok if (ch.isdigit() or ch in ".-+eE"))
                try:
                    nums.append(float(tok2))
                except Exception:
                    continue
            if len(nums) >= 2:
                break

        if len(nums) >= 2:
            z_val, phi_val = nums[0], nums[1]
        elif len(nums) == 1:
            z_val, phi_val = nums[0], float('nan')
        else:
            z_val, phi_val = float('nan'), float('nan')

        print(f"{f_hz} Hz -> Z={z_val}, phi={phi_val}")
        f.write(f"{int(f_hz)},{z_val},{phi_val}\n")

# Cleanup
inst.close()
rm.close()
print("Saved to", OUTPUT_FILE)