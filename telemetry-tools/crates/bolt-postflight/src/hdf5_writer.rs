#[cfg(feature = "hdf5")]
pub use real::HdfCollector;

#[cfg(not(feature = "hdf5"))]
pub use noop::HdfCollector;

#[cfg(feature = "hdf5")]
mod real {
    use std::collections::BTreeMap;

    use anyhow::{Context, Result};
    use bolt_codec::Sample;

    struct Cols {
        source: String,
        tick: Vec<u16>,
        ts: Vec<u32>,
        cols: BTreeMap<&'static str, Vec<f32>>,
    }

    // Accumulates columns in memory, then flushes to HDF5
    #[derive(Default)]
    pub struct HdfCollector {
        names: BTreeMap<String, Cols>,
    }

    impl HdfCollector {
        #[must_use]
        pub fn new() -> Self {
            Self::default()
        }

        pub fn add(&mut self, name: &str, source: &str, tick: u16, ts: u32, sample: &Sample) {
            let channels = sample.columns();
            if channels.is_empty() {
                return; // discrete/event payloads have no array
            }
            let entry = self.names.entry(name.to_string()).or_insert_with(|| Cols {
                source: source.to_string(),
                tick: Vec::new(),
                ts: Vec::new(),
                cols: BTreeMap::new(),
            });
            entry.tick.push(tick);
            entry.ts.push(ts);
            for (col, v) in channels {
                entry.cols.entry(col).or_default().push(v as f32);
            }
        }

        pub fn write(self, path: &str) -> Result<()> {
            let file = hdf5::File::create(path).with_context(|| format!("create {path}"))?;
            for (name, c) in self.names {
                let group = file
                    .create_group(&format!("{}/{}", c.source, name))
                    .with_context(|| format!("group {}/{name}", c.source))?;
                ds_u32(&group, "timestamp_us", &c.ts)?;
                ds_u16(&group, "tick", &c.tick)?;
                for (col, data) in c.cols {
                    ds_f32(&group, col, &data)?;
                }
            }
            Ok(())
        }
    }

    fn chunk_of(n: usize) -> usize {
        n.clamp(1, 65_536)
    }

    fn ds_f32(g: &hdf5::Group, name: &str, data: &[f32]) -> Result<()> {
        let ds = g
            .new_dataset::<f32>()
            .shape([data.len()])
            .chunk([chunk_of(data.len())])
            .deflate(4)
            .create(name)?;
        ds.write_raw(data)?;
        Ok(())
    }
    fn ds_u32(g: &hdf5::Group, name: &str, data: &[u32]) -> Result<()> {
        let ds = g
            .new_dataset::<u32>()
            .shape([data.len()])
            .chunk([chunk_of(data.len())])
            .deflate(4)
            .create(name)?;
        ds.write_raw(data)?;
        Ok(())
    }
    fn ds_u16(g: &hdf5::Group, name: &str, data: &[u16]) -> Result<()> {
        let ds = g
            .new_dataset::<u16>()
            .shape([data.len()])
            .chunk([chunk_of(data.len())])
            .deflate(4)
            .create(name)?;
        ds.write_raw(data)?;
        Ok(())
    }
}

#[cfg(not(feature = "hdf5"))]
mod noop {
    use anyhow::Result;
    use bolt_codec::Sample;

    #[derive(Default)]
    pub struct HdfCollector;

    impl HdfCollector {
        #[must_use]
        pub fn new() -> Self {
            HdfCollector
        }
        pub fn add(&mut self, _n: &str, _s: &str, _t: u16, _ts: u32, _sample: &Sample) {}

        pub fn write(self, _path: &str) -> Result<()> {
            eprintln!(
                "[postflight] HDF5 skipped (built without the `hdf5` feature) - manifest is complete; \
                 rebuild with --features hdf5 for the sensor arrays"
            );
            Ok(())
        }
    }
}
