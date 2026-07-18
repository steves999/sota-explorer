Place these two files in this folder before running:
  pio run --target uploadfs

  summits.csv  -- SOTA (+HEMA when available) summits for Summit mode
  parks.csv    -- WWFF/VKFF/ZLFF + POTA (AU/NZ) parks for Park mode

Generate both files by running:
  python filter_all_programmes.py

Do NOT upload the old zl_vk_summits.csv -- it has been replaced by summits.csv.
