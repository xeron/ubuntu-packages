<?xml version="1.0" standalone="yes"?>
<sch:schema xmlns:sch="http://purl.oclc.org/dsdl/schematron" xmlns:rng="http://relaxng.org/ns/structure/1.0">
  <sch:pattern xmlns="http://relaxng.org/ns/structure/1.0">
    <sch:rule context="/application">
      <sch:assert test="count(name[not(@xml:lang)]) &lt; 2">Excluding translations, at most one name element is permitted.</sch:assert>
      <sch:assert test="count(name) = count(name[not(@xml:lang=preceding-sibling::name/@xml:lang)])">At most one name element is permitted per language.</sch:assert>
    </sch:rule>
  </sch:pattern>
  <sch:pattern xmlns="http://relaxng.org/ns/structure/1.0">
    <sch:rule context="/application">
      <sch:assert test="count(summary[not(@xml:lang)]) &lt; 2">Excluding translations, at most one summary element is permitted.</sch:assert>
      <sch:assert test="count(summary) = count(summary[not(@xml:lang=preceding-sibling::summary/@xml:lang)])">At most one summary element is permitted per language.</sch:assert>
    </sch:rule>
  </sch:pattern>
  <sch:diagnostics/>
</sch:schema>

