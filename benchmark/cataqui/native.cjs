"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { performance } = require("node:perf_hooks");
const { createHash } = require("node:crypto");
const { parseArgs } = require("node:util");
const { values } = parseArgs({ options: {
    addon: { type: "string" }, fixtures: { type: "string" }, output: { type: "string" },
    blocks: { type: "string", default: "8" }, frames: { type: "string" },
} });
assert.ok(values.addon && values.fixtures && values.output);
assert.equal(process.platform, "linux");
assert.equal(process.arch, "x64");
assert.equal(process.versions.modules, "137");
const native = require(path.resolve(values.addon));
assert.equal(typeof native.Map.prototype.drawPrepared, "function");
assert.equal(typeof native.Map.prototype.setGeoJSONSourceData, "function");
const fixtures = path.resolve(values.fixtures);
const manifest = JSON.parse(fs.readFileSync(path.join(fixtures, "manifest.json")));
const originalStyle = JSON.parse(fs.readFileSync(path.join(fixtures, "production-style.json")));
const icons = JSON.parse(fs.readFileSync(path.join(fixtures, "icons.json")));
const resources = new Map();
const usedResources = new Set();
for (const resource of manifest.resources) {
    const bytes = fs.readFileSync(path.join(fixtures, resource.path));
    assert.equal(createHash("sha256").update(bytes).digest("hex"), resource.sha256);
    resources.set(resource.path, bytes);
}
const cases = manifest.scenarios.flatMap(scene => [1.333333333, 720 / 328].map(density => ({ ...scene, density: Math.fround(density), requestedDensity: density })));
const names = ["Mercado Municipal", "Praça Central", "Hospital Regional", "Escola Municipal", "Parque da Cidade", "Centro Cultural"];
function _features(scene, displacement) {
    const latitude = scene.center[1] * Math.PI / 180;
    const degreesPerPixel = 360 / (512 * 2 ** scene.zoom);
    return { type: "FeatureCollection", features: Array.from({ length: scene.zoom < 11 ? 0 : 96 }, (_, index) => ({
        type: "Feature",
        geometry: { type: "Point", coordinates: [
            scene.center[0] + ((index % 8) - 3.5 + displacement) * scene.width / 12 * degreesPerPixel,
            scene.center[1] + (Math.floor(index / 8) - 5.5) * scene.height / 16 * degreesPerPixel * Math.cos(latitude),
        ] },
        properties: { id: String(index), rank: index, fitSide: index % 8 >= 5 ? "left" : index % 8 <= 2 ? "right" : "both", name: icons[index % icons.length].id === "TRAIN_STATION" ? "" : names[index % names.length], icon: icons[index % icons.length].id },
    })) };
}
function _style(scene) {
    const style = structuredClone(originalStyle);
    style.sources.protomaps.tiles = ["cataqui://fixture/tiles/{z}/{x}/{y}.mvt"];
    style.glyphs = "cataqui://fixture/glyphs/{fontstack}/{range}.pbf";
    style.sources.landmarks = { type: "geojson", data: _features(scene, 0) };
    const layer = {
        id: "landmarks", type: "symbol", source: "landmarks", filter: ["in", "id", ...Array.from({ length: 96 }, (_, i) => String(i))],
        layout: {
            "symbol-placement": "point", "symbol-sort-key": ["get", "rank"], "symbol-z-order": "source",
            "icon-image": ["get", "icon"], "icon-size": 1, "icon-allow-overlap": false, "icon-ignore-placement": false,
            "text-field": ["get", "name"], "text-font": ["Noto Sans Bold"], "text-size": 10, "text-max-width": 8,
            "text-variable-anchor-offset": ["match", ["get", "fitSide"], "left", ["literal", ["right", [-1.6, 0]]], "right", ["literal", ["left", [1.6, 0]]], ["literal", ["left", [1.6, 0], "right", [-1.6, 0]]]],
            "text-justify": "center", "text-line-height": 1.2, "text-letter-spacing": -0.02,
            "text-allow-overlap": false, "text-ignore-placement": false,
        },
        paint: { "text-color": "#333333", "text-halo-color": "#ffffff", "text-halo-width": 1.1 },
    };
    const first = style.layers.findIndex(layer => layer.type === "symbol" && ["place", "places", "earth"].includes(layer["source-layer"]));
    style.layers.splice(first < 0 ? style.layers.length : first, 0, layer);
    return style;
}
function _create(scene) {
    return new native.Map({ mode: "static", ratio: scene.density, request: (request, callback) => {
        const url = new URL(request.url);
        assert.equal(url.protocol, "cataqui:");
        assert.equal(url.hostname, "fixture");
        const name = decodeURIComponent(url.pathname.slice(1));
        const bytes = resources.get(name);
        if (!bytes) return callback(new Error("Missing offline benchmark resource: " + name));
        usedResources.add(name);
        callback(undefined, { data: bytes });
    } });
}
function _render(map, scene, prepareOnly) {
    return new Promise((resolve, reject) => map.render({ zoom: scene.zoom, center: scene.center, width: scene.width, height: scene.height, ...(prepareOnly ? { prepareOnly: true } : {}) }, (error, result) => error ? reject(error) : resolve(result)));
}
function _draw(map) { return new Promise((resolve, reject) => map.drawPrepared((error, bytes) => error ? reject(error) : resolve(bytes))); }
async function _prepared(map, scene) {
    assert.equal(await _render(map, scene, true), true);
    const features = map.queryRenderedFeatures([[0, 0], [scene.width, scene.height]], { layers: ["landmarks"] });
    assert.ok(features.length <= 96);
    const bytes = await _draw(map);
    assert.equal(bytes.length, Math.floor(scene.width * scene.density) * Math.floor(scene.height * scene.density) * 4);
    return { bytes, ids: [...new Set(features.map(feature => feature.properties.id))].sort() };
}
function _frame(key, scene, result) {
    if (values.frames) {
        fs.mkdirSync(values.frames, { recursive: true });
        fs.writeFileSync(path.join(values.frames, key + ".rgba"), result.bytes);
    }
    return { key, width: Math.floor(scene.width * scene.density), height: Math.floor(scene.height * scene.density), sha256: createHash("sha256").update(result.bytes).digest("hex"), visibleIds: result.ids };
}
(async () => {
    const samples = [];
    const frames = [];
    const blocks = Number(values.blocks);
    assert.ok(Number.isInteger(blocks) && blocks >= 1 && blocks <= 100);
    for (let block = -1; block < blocks; block++) {
        const order = block % 2 === 0 ? cases : [...cases].reverse();
        for (const scene of order) {
            const key = `${scene.id}-${scene.density.toFixed(6)}`;
            const record = { block, case: key, phases: {} };
            let map;
            try {
                let start = performance.now();
                map = _create(scene);
                map.load(_style(scene));
                for (const icon of scene.zoom >= 11 ? icons : []) map.addImage(icon.id, Buffer.from(icon.pixels, "base64"), { width: icon.width, height: icon.height, pixelRatio: icon.density, sdf: false });
                const cold = await _prepared(map, scene);
                record.phases.cold = performance.now() - start;
                start = performance.now();
                map.setFilter("landmarks", ["in", "id", ...Array.from({ length: 96 }, (_, i) => String(i))]);
                const warm = await _prepared(map, scene);
                record.phases.warm = performance.now() - start;
                assert.equal(createHash("sha256").update(cold.bytes).digest("hex"), createHash("sha256").update(warm.bytes).digest("hex"));
                const second = _create(scene);
                try {
                    second.load(_style(scene));
                    for (const icon of scene.zoom >= 11 ? icons : []) second.addImage(icon.id, Buffer.from(icon.pixels, "base64"), { width: icon.width, height: icon.height, pixelRatio: icon.density, sdf: false });
                    await _prepared(second, scene);
                    start = performance.now();
                    await Promise.all([_prepared(map, scene), _prepared(second, scene)]);
                    record.phases.parallel = performance.now() - start;
                } finally { second.release(); }
                start = performance.now();
                map.setGeoJSONSourceData("landmarks", _features(scene, 0.1));
                const updated = await _prepared(map, scene);
                record.phases.update = performance.now() - start;
                start = performance.now();
                map.setFilter("landmarks", ["in", "id", ...updated.ids.filter((_, index) => index % 2 === 0)]);
                const filtered = await _render(map, scene, false);
                record.phases.filtered = performance.now() - start;
                assert.equal(filtered.length, updated.bytes.length);
                start = performance.now();
                const resized = { ...scene, width: scene.width + 12 };
                const resizedResult = await _prepared(map, resized);
                record.phases.resize = performance.now() - start;
                start = performance.now();
                map.release(); map = undefined;
                record.phases.release = performance.now() - start;
                record.maxRssKiB = process.resourceUsage().maxRSS;
                if (block === 0) {
                    frames.push(_frame(key + "-base", scene, warm));
                    frames.push(_frame(key + "-update", scene, updated));
                    frames.push(_frame(key + "-resize", resized, resizedResult));
                }
                if (block >= 0) samples.push(record);
            } finally { if (map) map.release(); }
        }
    }
    fs.writeFileSync(values.output, JSON.stringify({ schemaVersion: 1, platform: process.platform, arch: process.arch, node: process.version, abi: process.versions.modules, samples, frames, usedResources: [...usedResources].sort() }, null, 2) + "\n");
    console.log(JSON.stringify({ samples: samples.length, output: values.output }));
})().catch(error => { console.error(error); process.exitCode = 1; });
