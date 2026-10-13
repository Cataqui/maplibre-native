const assert = require('node:assert/strict');
const { test } = require('node:test');
const native = require('../../index');

const empty = { type: 'FeatureCollection', features: [] };
const style = {
    version: 8,
    sources: { points: { type: 'geojson', data: empty } },
    layers: [
        { id: 'background', type: 'background', paint: { 'background-color': '#ffffff' } },
        { id: 'points', type: 'circle', source: 'points', paint: { 'circle-radius': 8 } }
    ]
};
function render(map, options) {
    return new Promise((resolve, reject) => map.render(options, (error, result) => error ? reject(error) : resolve(result)));
}
function draw(map) {
    return new Promise((resolve, reject) => map.drawPrepared((error, pixels) => error ? reject(error) : resolve(pixels)));
}

test('static placement is consumed once and source updates invalidate it', async () => {
    const map = new native.Map({ mode: 'static', ratio: 1.25 });
    try {
        map.load(style);
        const options = { center: [0, 0], zoom: 2, width: 128, height: 96, prepareOnly: true };
        assert.equal(await render(map, options), true);
        assert.equal((await draw(map)).byteLength, 160 * 120 * 4);
        await assert.rejects(draw(map), /prepared/i);
        assert.equal(await render(map, options), true);
        map.setGeoJSONSourceData('points', {
            type: 'FeatureCollection',
            features: [{ type: 'Feature', properties: {}, geometry: { type: 'Point', coordinates: [0, 0] } }]
        });
        await assert.rejects(draw(map), /prepared/i);
        assert.equal(await render(map, options), true);
        assert.equal(map.queryRenderedFeatures([[0, 0], [128, 96]], { layers: ['points'] }).length, 1);
        assert.equal((await draw(map)).byteLength, 160 * 120 * 4);
        assert.throws(() => map.setGeoJSONSourceData('missing', empty), /source/i);
    } finally {
        map.release();
    }
});
