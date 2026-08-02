<template>
  <div id="status">
    <div v-if="error" class="banner banner-error" role="alert">
      Device unreachable — retrying&hellip;
    </div>

    <template v-if="loaded && probes.length">
      <div class="probe-cards">
        <div
          v-for="p in probes"
          :key="p.key"
          class="probe-card"
          :class="stale ? 'state-stale' : `state-${p.state}`"
          :style="{ '--probe-color': p.color.line }"
        >
          <div class="probe-card-head">
            <span class="probe-dot" />
            <span class="probe-label">{{ p.label }}</span>
            <span
              v-if="stale"
              class="probe-badge stale-badge"
              :title="staleTitle"
            >
              Stale
            </span>
            <span
              v-else-if="billows && p.isControl"
              class="probe-badge billows-badge"
              title="Billows fan connected"
            >
              <svg
                class="billows-icon"
                viewBox="0 0 24 24"
                fill="currentColor"
                aria-hidden="true"
              >
                <path
                  d="M12,11A1,1 0 0,0 11,12A1,1 0 0,0 12,13A1,1 0 0,0 13,12A1,1 0 0,0 12,11M12.5,2C17,2 17.11,5.57 14.75,6.75C13.76,7.24 13.32,8.29 13.13,9.22C13.61,9.42 14.03,9.73 14.35,10.13C18.05,8.13 22.03,8.92 22.03,12.5C22.03,17 18.46,17.1 17.28,14.73C16.78,13.74 15.72,13.3 14.79,13.11C14.59,13.59 14.28,14 13.88,14.34C15.87,18.03 15.08,22 11.5,22C7,22 6.91,18.42 9.27,17.24C10.25,16.75 10.69,15.71 10.89,14.79C10.4,14.59 9.97,14.27 9.65,13.87C5.96,15.85 2,15.07 2,11.5C2,7 5.56,6.89 6.74,9.26C7.24,10.25 8.29,10.68 9.22,10.87C9.41,10.39 9.73,9.97 10.14,9.65C8.15,5.96 8.94,2 12.5,2Z"
                />
              </svg>
              Billows
            </span>
            <span v-else class="probe-badge">{{ stateLabel(p.state) }}</span>
          </div>
          <div class="probe-temp">
            {{ fmt(p.current_temp) }}<span class="probe-unit">°F</span>
          </div>
          <div class="probe-footer">
            <div class="stat-grid extremes">
              <span class="lbl">Max/Min</span>
              <span class="g" title="Session high">&uarr;</span>
              <span class="v">{{ fmt(p.hi) }}°</span>
              <span class="g" title="Session low">&darr;</span>
              <span class="v">{{ fmt(p.lo) }}°</span>
            </div>
            <div
              v-if="billows && p.isControl"
              class="stat-grid alarm"
              title="Billows set temperature"
            >
              <span class="lbl">Set</span>
              <span class="g" title="Billows target">&#9678;</span>
              <span class="v">{{ fmt(p.alarm_max) }}°</span>
              <!-- Blank third row so the set block matches the alarm block's
                   height, keeping the session high/low column aligned with the
                   other probe cards. -->
              <span class="g" aria-hidden="true">&nbsp;</span>
              <span class="v" aria-hidden="true">&nbsp;</span>
            </div>
            <div v-else class="stat-grid alarm">
              <span class="lbl">Alarm</span>
              <span class="g" title="High alarm">&#9650;</span>
              <span class="v">{{ fmt(p.alarm_max) }}°</span>
              <span class="g" title="Low alarm">&#9660;</span>
              <span class="v">{{ fmt(p.alarm_min) }}°</span>
            </div>
          </div>
        </div>
      </div>

      <section class="panel chart-panel">
        <h2 class="panel-title" :class="{ open: chartOpen }">
          <button
            class="panel-toggle"
            type="button"
            :aria-expanded="chartOpen"
            aria-controls="canvasWrapper"
            @click="toggleChart"
          >
            <span>Temperature history</span>
            <svg
              class="chevron"
              :class="{ open: chartOpen }"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              aria-hidden="true"
            >
              <path d="M6 9l6 6 6-6" />
            </svg>
          </button>
        </h2>
        <div
          v-show="chartOpen"
          id="canvasWrapper"
          style="position: relative; height: 60vh"
          role="img"
          :aria-label="chartSummary"
        >
          <Line
            :data="chartData"
            :options="options"
            :plugins="[alarmLinesPlugin, staleBadgePlugin, gapMarkersPlugin]"
          />
        </div>
      </section>
    </template>

    <h3 v-else-if="loaded">
      <br />
      No temperature information received
    </h3>
    <h3 v-else>
      <br />
      Loading&hellip;
    </h3>
  </div>
</template>

<script>
import { Line } from "vue-chartjs"
import {
  Chart as ChartJS,
  Title,
  Tooltip,
  Legend,
  LineElement,
  CategoryScale,
  LinearScale,
  PointElement,
} from "chart.js"
import { getJSON } from "../api"
import { isDark } from "../theme"
import { packetAgeMs, dataStale } from "../device"

ChartJS.register(
  Title,
  Tooltip,
  Legend,
  LineElement,
  CategoryScale,
  LinearScale,
  PointElement
)

// The base station transmits every 30 s, so each history sample is 30 s apart.
// The API sends no timestamps, so the x-axis is labelled by relative age
// ("minutes before now") rather than a wall clock we can't honestly claim.
const SAMPLE_SECONDS = 30

// Chart canvas colors per theme (canvas can't read CSS custom properties).
const CHART_COLORS = {
  light: { tick: "#667", grid: "rgba(0, 0, 0, 0.1)", legend: "#2c3e50" },
  dark: { tick: "#99a3b1", grid: "rgba(255, 255, 255, 0.12)", legend: "#e6e8ec" },
}

// Shared palette so the probe cards and chart lines always match. Vivid,
// well-separated hues (Tailwind 500s) that stay sharp on light and dark
// backgrounds; `faint` is the same hue at lower alpha for the min-alarm line.
// `dash`/`point` give each probe a distinct line texture and legend marker so
// the series stay distinguishable without relying on color (colorblind-safe).
const PROBE_COLORS = [
  { line: "#ef4444", faint: "rgba(239, 68, 68, 0.5)", dash: [], point: "circle" }, // red
  { line: "#3b82f6", faint: "rgba(59, 130, 246, 0.5)", dash: [7, 3], point: "rect" }, // blue
  { line: "#22c55e", faint: "rgba(34, 197, 94, 0.5)", dash: [2, 3], point: "triangle" }, // green
  { line: "#f59e0b", faint: "rgba(245, 158, 11, 0.5)", dash: [9, 3, 2, 3], point: "rectRot" }, // amber
]

const probeKeys = (data) =>
  Object.keys(data).filter((k) => /^probe_\d+$/.test(k))

// A history entry is either a temperature (number) or an outage marker. The
// firmware writes {gap: seconds}; a bare null is treated as an unknown-length
// gap (one sample wide) for resilience.
const isGapEntry = (v) => typeof v !== "number"
const gapSeconds = (v) =>
  v && typeof v === "object" && typeof v.gap === "number" ? v.gap : SAMPLE_SECONDS

// Reconstruct a real-time x-axis (minutes before now) from a probe's history.
// Normal samples are SAMPLE_SECONDS apart; a gap entry advances the clock by its
// stored duration, so outages render to scale rather than collapsing to a single
// step. Returns per-index x plus the gap spans (for shading). All probes share
// the same gap structure, so this is computed once from one probe's history.
function buildTimeline(history) {
  const secs = []
  const gaps = []
  let t = 0
  let prevWasGap = false
  let first = true
  history.forEach((v) => {
    if (isGapEntry(v)) {
      const d = gapSeconds(v)
      const from = t
      secs.push(t + d / 2) // marker (null point) sits mid-gap
      t += d
      gaps.push({ from, to: t })
      prevWasGap = true
      return
    }
    // The gap already advanced the clock to this sample's time, so a sample
    // right after a gap adds no further step.
    if (first) first = false
    else if (!prevWasGap) t += SAMPLE_SECONDS
    prevWasGap = false
    secs.push(t)
  })
  const total = secs.length ? secs[secs.length - 1] : 0
  return {
    xs: secs.map((s) => (s - total) / 60),
    gaps: gaps.map((g) => ({
      from: (g.from - total) / 60,
      to: (g.to - total) / 60,
    })),
  }
}

// Inline plugin: draw each probe's alarm min/max as dashed threshold lines.
// Reads alarm data from chart.options.plugins.alarmLines so it stays in sync
// with the reactive options object. No external dependency required.
const alarmLinesPlugin = {
  id: "alarmLines",
  afterDatasetsDraw(chart) {
    const cfg = chart.options.plugins.alarmLines
    if (!cfg || !cfg.probes) return
    const { ctx, chartArea, scales } = chart
    const y = scales.y
    ctx.save()
    ctx.font = "10px sans-serif"
    ctx.textBaseline = "middle"
    cfg.probes.forEach((p, i) => {
      // Hide a probe's threshold lines when its series is toggled off in the
      // legend (probe order matches dataset order).
      if (!chart.isDatasetVisible(i)) return
      // The Billows control probe shows a single set-temp line (its high alarm
      // carries the target) with the same ◎ glyph as the card, instead of the
      // ▲/▼ alarm pair.
      const lines =
        cfg.billows && p.isControl
          ? [["set", p.alarm_max, p.color.line, "◎"]]
          : [
              ["max", p.alarm_max, p.color.line, "▲"],
              ["min", p.alarm_min, p.color.faint, "▼"],
            ]
      lines.forEach(([kind, val, stroke, glyph]) => {
        if (val == null) return
        const py = y.getPixelForValue(val)
        if (py < chartArea.top || py > chartArea.bottom) return
        ctx.beginPath()
        ctx.setLineDash(kind === "min" ? [2, 4] : [6, 4])
        ctx.lineWidth = 1
        ctx.strokeStyle = stroke
        ctx.moveTo(chartArea.left, py)
        ctx.lineTo(chartArea.right, py)
        ctx.stroke()
        ctx.setLineDash([])
        const label = `${p.short}${glyph}${Math.round(val)}`
        ctx.fillStyle = stroke
        ctx.textAlign = "right"
        ctx.fillText(label, chartArea.right - 4, py - 6)
      })
    })
    ctx.restore()
  },
}

// Inline plugin: when the readings are stale (base station gone silent), stamp
// an amber "No signal · Nm" pill in the chart's top-right corner. A corner badge
// stays readable at any history length, unlike a time-proportional gap (a few
// minutes of silence is an invisible sliver on a multi-hour axis). Reads its
// config from chart.options.plugins.staleBadge.
const staleBadgePlugin = {
  id: "staleBadge",
  afterDatasetsDraw(chart) {
    const cfg = chart.options.plugins.staleBadge
    if (!cfg || !cfg.stale) return
    const { ctx, chartArea } = chart
    const label = cfg.ago ? `No signal · ${cfg.ago}` : "No signal"
    ctx.save()
    ctx.font = "bold 11px sans-serif"
    const padX = 8
    const w = ctx.measureText(label).width + padX * 2
    const h = 20
    const x = chartArea.right - w - 6
    const y = chartArea.top + 6
    ctx.fillStyle = cfg.fill
    if (ctx.roundRect) {
      ctx.beginPath()
      ctx.roundRect(x, y, w, h, 10)
      ctx.fill()
    } else {
      ctx.fillRect(x, y, w, h)
    }
    ctx.fillStyle = cfg.text
    ctx.textAlign = "center"
    ctx.textBaseline = "middle"
    ctx.fillText(label, x + w / 2, y + h / 2 + 0.5)
    ctx.restore()
  },
}

// Inline plugin: shade each historical outage as a band scaled to its duration,
// with dashed edges. The line already breaks at the gap (null point + spanGaps),
// but the band makes the outage — and how long it lasted — legible at a glance.
// A minimum 2px width keeps very short gaps visible. Reads the spans (in
// minutes-ago) from chart.options.plugins.gapMarkers.
const gapMarkersPlugin = {
  id: "gapMarkers",
  afterDatasetsDraw(chart) {
    const cfg = chart.options.plugins.gapMarkers
    if (!cfg || !cfg.gaps || !cfg.gaps.length) return
    const { ctx, chartArea, scales } = chart
    ctx.save()
    cfg.gaps.forEach((g) => {
      const left = Math.max(scales.x.getPixelForValue(g.from), chartArea.left)
      const right = Math.min(scales.x.getPixelForValue(g.to), chartArea.right)
      if (right <= chartArea.left || left >= chartArea.right) return
      const w = Math.max(right - left, 2)
      ctx.fillStyle = cfg.fill
      ctx.fillRect(left, chartArea.top, w, chartArea.bottom - chartArea.top)
      ctx.strokeStyle = cfg.stroke
      ctx.setLineDash([3, 3])
      ctx.lineWidth = 1
      ctx.beginPath()
      ctx.moveTo(left, chartArea.top)
      ctx.lineTo(left, chartArea.bottom)
      ctx.moveTo(left + w, chartArea.top)
      ctx.lineTo(left + w, chartArea.bottom)
      ctx.stroke()
      ctx.setLineDash([])
    })
    ctx.restore()
  },
}

export default {
  name: "LineChart",
  // eslint-disable-next-line
  components: { Line },
  data: () => ({
    loaded: false,
    error: false,
    stopped: false,
    data: null,
    alarmLinesPlugin,
    staleBadgePlugin,
    gapMarkersPlugin,
    // Chart panel collapse state, remembered across visits. Defaults open.
    chartOpen: (() => {
      try {
        return localStorage.getItem("chartOpen") !== "false"
      } catch (e) {
        return true
      }
    })(),
  }),
  computed: {
    probes() {
      if (!this.data) return []
      return probeKeys(this.data).map((k, i, arr) => {
        const hist = (this.data[k].history || []).filter(
          (v) => typeof v === "number"
        )
        // The last probe (P2 on X2, P4 on X4) is the Billows fan-control probe.
        // When a Billows is attached its high alarm carries the pit set/target
        // temperature, shown in place of the alarm high/low pair — and its
        // alarm-based OK/HIGH/LOW state no longer applies (it's a setpoint, not
        // an alarm), so it isn't flagged when the pit sits above target.
        const isControl = i === arr.length - 1
        const isSetpoint = isControl && !!this.data.billows
        return {
          key: k,
          short: `P${k.split("_")[1]}`,
          label: `Probe ${k.split("_")[1]}`,
          color: PROBE_COLORS[i % PROBE_COLORS.length],
          current_temp: this.data[k].current_temp,
          alarm_min: this.data[k].alarm_min,
          alarm_max: this.data[k].alarm_max,
          // Session extremes observed over the collected history.
          hi: hist.length ? Math.max(...hist) : null,
          lo: hist.length ? Math.min(...hist) : null,
          isControl,
          state: isSetpoint ? "ok" : this.stateOf(this.data[k]),
        }
      })
    },
    billows() {
      return !!(this.data && this.data.billows)
    },
    // Readings are stale when the base station has gone silent (shared threshold
    // from the device store — the same signal the header pill uses).
    stale() {
      return dataStale.value
    },
    // Compact "3m" / "45s" age for the stale badge tooltip and chart label.
    staleAgo() {
      if (typeof packetAgeMs.value !== "number") return ""
      const s = Math.round(packetAgeMs.value / 1000)
      return s < 60 ? `${s}s` : `${Math.floor(s / 60)}m`
    },
    staleTitle() {
      return `No signal from the base station — showing the last reading${
        this.staleAgo ? ` from ${this.staleAgo} ago` : ""
      }`
    },
    // Real-time x-axis + gap spans, computed once from one probe's history (all
    // probes share the same gap structure). Feeds both the datasets and the gap
    // shading, and re-derives when the polled data changes.
    timeline() {
      const keys = this.data ? probeKeys(this.data) : []
      if (!keys.length) return { xs: [], gaps: [] }
      return buildTimeline(this.data[keys[0]].history || [])
    },
    // Chart datasets: each probe's temperatures placed on the shared real-time
    // axis, with gap entries rendered as null points (breaking the line).
    chartData() {
      if (!this.data) return null
      const keys = probeKeys(this.data)
      if (!keys.length) return null
      const xs = this.timeline.xs
      return {
        datasets: keys.map((k, i) => {
          const history = Array.isArray(this.data[k].history)
            ? this.data[k].history
            : []
          const color = PROBE_COLORS[i % PROBE_COLORS.length]
          return {
            label: `Probe ${k.split("_")[1]}`,
            data: history.map((v, idx) => ({
              x: xs[idx] ?? 0,
              y: typeof v === "number" ? v : null,
            })),
            fill: false,
            borderColor: color.line,
            // Break the line at gap markers rather than bridging the outage.
            spanGaps: false,
            // Distinct texture + legend marker per probe (colorblind-safe).
            borderDash: color.dash,
            pointStyle: color.point,
            tension: 0,
            pointRadius: 2,
          }
        }),
      }
    },
    // Text alternative for the canvas chart (which screen readers can't read).
    chartSummary() {
      if (!this.probes.length) return "Temperature history chart"
      const parts = this.probes.map((p) => {
        const state = p.state === "ok" ? "" : ` (${this.stateLabel(p.state)})`
        return `${p.label} ${this.fmt(p.current_temp)}°F${state}`
      })
      return `Temperature history over about 4 hours. Latest readings: ${parts.join(
        ", "
      )}.`
    },
    options() {
      const alarmVals = this.probes.flatMap((p) => [p.alarm_min, p.alarm_max])
      const finite = alarmVals.filter((v) => typeof v === "number")
      const c = isDark.value ? CHART_COLORS.dark : CHART_COLORS.light
      // Once the history spans more than two hours, minute-scale ticks get
      // crowded and hard to read, so switch the axis (and tooltip) to hours.
      const spanMinutes = this.chartData
        ? Math.max(
            0,
            ...this.chartData.datasets.flatMap((d) => d.data.map((pt) => -pt.x))
          )
        : 0
      const useHours = spanMinutes > 120
      const ago = (minutes) => {
        if (minutes === 0) return "now"
        if (!useHours) return `${minutes}m`
        const total = Math.round(minutes)
        return `${Math.floor(total / 60)}:${String(total % 60).padStart(2, "0")}`
      }
      return {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
          legend: {
            position: "bottom",
            labels: { color: c.legend, usePointStyle: true },
          },
          tooltip: {
            callbacks: {
              title: (items) => {
                const minutes = -items[0].parsed.x
                return minutes === 0 ? "now" : `${ago(minutes)} ago`
              },
            },
          },
          // Consumed by alarmLinesPlugin above.
          alarmLines: { probes: this.probes, billows: this.billows },
          // Consumed by staleBadgePlugin above.
          staleBadge: {
            stale: this.stale,
            ago: this.staleAgo,
            fill: isDark.value ? "#eab308" : "#b45309",
            text: isDark.value ? "#1a1400" : "#ffffff",
          },
          // Consumed by gapMarkersPlugin above. A neutral darkened band (rather
          // than a bright highlight) reads as a dead/no-data region.
          gapMarkers: {
            gaps: this.timeline.gaps,
            fill: isDark.value ? "rgba(0, 0, 0, 0.35)" : "rgba(0, 0, 0, 0.10)",
            stroke: isDark.value ? "rgba(255, 255, 255, 0.22)" : "rgba(0, 0, 0, 0.28)",
          },
        },
        scales: {
          x: {
            type: "linear",
            title: {
              display: true,
              text: "Time ago",
              color: c.tick,
            },
            border: { display: true, color: c.grid },
            grid: { display: true, drawOnChartArea: true, color: c.grid },
            ticks: {
              color: c.tick,
              callback: (v) => ago(Math.abs(Number(v))),
            },
          },
          y: {
            border: { display: true, color: c.grid },
            grid: { color: c.grid },
            ticks: { color: c.tick },
            suggestedMin: finite.length ? Math.min(...finite) - 10 : undefined,
            suggestedMax: finite.length ? Math.max(...finite) + 10 : undefined,
          },
        },
      }
    },
  },
  created: function () {
    this.poll()
  },
  beforeUnmount: function () {
    this.stopped = true
    clearTimeout(this.timer)
  },
  methods: {
    toggleChart() {
      this.chartOpen = !this.chartOpen
      try {
        localStorage.setItem("chartOpen", String(this.chartOpen))
      } catch (e) {
        /* storage unavailable — keep in-memory state only */
      }
    },
    fmt(v) {
      return typeof v === "number" ? Math.round(v * 10) / 10 : "--"
    },
    stateOf(probe) {
      const t = probe.current_temp
      if (typeof t !== "number") return "ok"
      if (typeof probe.alarm_max === "number" && t >= probe.alarm_max)
        return "high"
      if (typeof probe.alarm_min === "number" && t <= probe.alarm_min)
        return "low"
      return "ok"
    },
    stateLabel(state) {
      return state === "high" ? "HIGH" : state === "low" ? "LOW" : "OK"
    },
    async poll() {
      await this.getData()
      if (!this.stopped) this.timer = setTimeout(this.poll, 30000)
    },
    async getData() {
      try {
        const data = await getJSON("data")
        this.data = data
        this.error = false
      } catch (error) {
        this.error = true
      } finally {
        this.loaded = true
      }
    },
  },
}
</script>

<style scoped>
.banner {
  max-width: 720px;
  margin: 1em auto;
  padding: 0.75em 1em;
  border-radius: 0.5em;
  text-align: center;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  font-weight: bold;
}
.banner-error {
  background-color: var(--alarm-high-bg);
  color: var(--alarm-high-fg);
}

.probe-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 1em;
  max-width: 720px;
  margin: 1em auto 1.25em;
  padding: 0 1em;
  box-sizing: border-box;
}

/* Chart lives in its own surface panel (matching the cards) with a header and
   a collapse toggle, so it reads as a distinct module rather than floating. */
.panel {
  max-width: 720px;
  margin: 0 auto 1.5em;
  padding: 0.6em 1em 0.9em;
  box-sizing: border-box;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 0.5em;
  box-shadow: 0 0 1em var(--shadow);
}
.panel-title {
  margin: 0;
  /* Reset the h2's default 1.5em so the button's 0.72em resolves against the
     base size and matches the "Now" section label exactly. */
  font-size: inherit;
}
/* Hairline divider between header and chart, only while expanded. */
.panel-title.open {
  border-bottom: 1px solid var(--border);
  padding-bottom: 0.5em;
  margin-bottom: 0.7em;
}
/* The whole header row is the toggle — a large, forgiving tap target. */
.panel-toggle {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5em;
  width: 100%;
  border: none;
  background: transparent;
  cursor: pointer;
  padding: 0.15em 0;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  font-size: 0.72em;
  font-weight: bold;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-muted);
  text-align: left;
}
.chevron {
  width: 22px;
  height: 22px;
  display: block;
  flex-shrink: 0;
  transition: transform 0.2s ease;
}
.chevron.open {
  transform: rotate(180deg);
}
@media (prefers-reduced-motion: reduce) {
  .chevron {
    transition: none;
  }
}

.probe-card {
  border: 1px solid var(--border);
  border-left: 4px solid var(--probe-color);
  border-radius: 0.5em;
  padding: 0.75em 1em;
  background: var(--surface);
  box-shadow: 0 0 1em var(--shadow);
  font-family: Avenir, Helvetica, Arial, sans-serif;
  color: var(--text);
}
.probe-card.state-high {
  background-color: var(--alarm-high-bg);
}
.probe-card.state-low {
  background-color: var(--alarm-low-bg);
}

.probe-card-head {
  display: flex;
  align-items: center;
  gap: 0.4em;
  font-size: 0.85em;
}
.probe-dot {
  width: 0.7em;
  height: 0.7em;
  border-radius: 50%;
  background-color: var(--probe-color);
}
.probe-label {
  font-weight: bold;
  white-space: nowrap;
}
.probe-badge {
  margin-left: auto;
  font-size: 0.7em;
  font-weight: bold;
  padding: 0.1em 0.5em;
  border-radius: 1em;
  background-color: var(--border);
  color: var(--text-muted);
}
/* Solid, theme-stable badge colors. The alarm/accent foreground tokens turn
   light in dark mode (they're tuned as text-on-dark), so white text on them
   fails contrast. These stay dark enough for legible white text in both
   themes. */
.state-high .probe-badge {
  background-color: #c62828;
  color: #fff;
}
.state-low .probe-badge {
  background-color: #1d63c9;
  color: #fff;
}
/* Stale: base station gone silent, readings are old. Amber badge matches the
   header "no signal" pill (--pill-warn-*), and the temperature is muted to read
   as "not live". No alarm tint is applied (state-stale replaces state-high/low)
   since the alarm state is no longer current. */
.stale-badge {
  background-color: var(--pill-warn-bg);
  color: var(--pill-warn-fg);
}
.probe-card.state-stale .probe-temp {
  color: var(--text-muted);
  opacity: 0.5;
}

.probe-temp {
  font-size: 2em;
  font-weight: bold;
  line-height: 1.1;
  margin: 0.15em 0;
}
.probe-unit {
  font-size: 0.5em;
  font-weight: normal;
  margin-left: 0.15em;
  color: var(--text-muted);
}
.probe-footer {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  gap: 0.75em;
  margin-top: 0.3em;
  color: var(--text-muted);
}
.stat-grid {
  display: grid;
  grid-template-columns: auto auto;
  column-gap: 0.3em;
  row-gap: 0.05em;
  align-items: baseline;
  white-space: nowrap;
  line-height: 1.3;
  /* Both blocks share one size so their three rows are equal height and line
     up row-for-row (Min/Max ↔ Alarm label, and each value across from its
     counterpart). */
  font-size: 0.78em;
}
.stat-grid .v {
  text-align: right;
  font-variant-numeric: tabular-nums;
}
/* Column-header label above each stat block (Min/Max on the left, Alarm/Set on
   the right), aligned to each block's outer edge. */
.stat-grid .lbl {
  grid-column: 1 / -1;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  opacity: 0.7;
}
.extremes .lbl {
  text-align: left;
}
.alarm .lbl {
  text-align: right;
}

/* Billows status pill, shown in the control probe's card head in place of the
   OK/HIGH/LOW badge. Inherits the .probe-badge pill shape; recolored to the
   accent to read as "fan active". */
.billows-badge {
  display: inline-flex;
  align-items: center;
  gap: 0.25em;
  padding: 0.1em 0.45em 0.1em 0.4em;
  white-space: nowrap;
  /* Deep green (the app's solid "ok" pill color) rather than the light accent,
     which fails white-text contrast in both themes. */
  background-color: var(--pill-ok-bg);
  color: var(--pill-ok-fg);
}
.billows-icon {
  width: 1em;
  height: 1em;
  display: block;
  animation: billows-spin 4s linear infinite;
}
@keyframes billows-spin {
  to {
    transform: rotate(360deg);
  }
}
@media (prefers-reduced-motion: reduce) {
  .billows-icon {
    animation: none;
  }
}
</style>
