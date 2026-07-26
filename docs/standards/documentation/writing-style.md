# Writing Style

Applies to prose we write: documents under `docs/`, ADRs, ICDs, commit
bodies, pull request descriptions, and external deliverables (for this
project: the SED). For code comments the rule is already in
[cpp.md](../coding/cpp.md#comments) and this page does not repeat it.

It applies to everyone, not only to text produced with an assistant. The
patterns below are not exclusive to machines. They are what padding looks
like whoever wrote it.

## Why we care

An external reviewer reads our documents to
find out whether we understand our own design. Padding does not read as
polish. It reads as someone covering a gap, and it costs us the benefit
of the doubt on the sentences that do carry content.

The failure is mostly not punctuation. It is
[sensationalised, synthesised and vaguely attributed statements](https://en.wikipedia.org/wiki/Wikipedia_talk:Signs_of_AI_writing):
text that asserts significance instead of information, and that attributes
claims to nobody. The rules below are ordered accordingly, content first.

## Content

**Every claim names where it comes from.** A file, a section, an ADR, a
datasheet, a measurement. Not "industry best practice", not "studies
show", not "it is generally accepted". If we cannot name the source, we
either measure it or write that we have not.

**Do not assert significance.** Delete sentences whose only content is
that something matters, is important, is a key part of, or plays a role
in. If the thing matters, the surrounding facts show it. If they do not,
the sentence is covering for them.

**A paragraph adds information or goes away.** A paragraph that restates
its heading, or summarises the section it ends, is filler. Sections end
when the information ends.

**Say what we do not know.** An open question, a value still to be
measured, a check that does not exist yet: write it. Uncertainty is
content, and P-4 is the same rule applied to telemetry. A document that
sounds complete about something unfinished is the written form of a
silently substituted measurement.

## Shape

Short list, because a long one causes its own problem. See the caution
below.

The source guide is explicit that the em dash is
[most useful in combination with other indicators, not by itself](https://flowingdata.com/2025/10/20/signs-of-ai-writing-on-wikipedia/),
and an editor on its talk page reports having
[stopped looking for it altogether](https://en.wikipedia.org/wiki/Wikipedia_talk:Signs_of_AI_writing).
We keep the rule because one unambiguous, checkable convention is worth
having, and because the replacement usually reads better in technical
prose. If it ever costs more than it buys, drop it and keep the content
rules, which are the ones that matter.

**A bold lead-in must be followed by new information.** Our house pattern
`**Bounded execution time.** ...` is fine when what follows says
something the bold phrase did not. It is filler when the sentence
restates the phrase.

```
Bad:   **Bounded execution time.** The execution time is bounded.
Good:  **Bounded execution time.** Every I2C, UART and CAN call takes a
       deadline, so a stuck peripheral costs one tick of data rather than
       the loop.
```

**Lists have the number of items they have.** Not three because three
sounds finished. If there are two reasons, give two.

**No stock openers.** "It is important to note", "It is worth
mentioning", "In today's", "In conclusion". They add length and no
content. The sentence after them is usually the sentence.

## A caution against over-correcting

Do not try to eliminate every possible tell. Guidance on this is
consistent and worth heeding:
[too many restrictions make the result stiff or generic](https://www.blakestockton.com/takeaways-from-wikipedias-signs-of-ai-writing-2/),
which reads as machine-written in a different way. The Wikipedia guide
itself is explicit that it is
[not a ban on particular words or punctuation](https://flowingdata.com/2025/10/20/signs-of-ai-writing-on-wikipedia/)
and that its signs are useful in combination rather than as single
tripwires.

So this page bans one punctuation mark and four stock openers, and
otherwise describes what a sentence has to do. Words like *robust*,
*significant* or *critical* are not forbidden: `robust to bus noise` is a
measurable claim, while `a robust architecture` is not. The test is
whether the word carries information in that sentence.

## Worked examples

From our own domain, in the direction we want.

```
Bad:   The deterministic tick architecture plays a vital role in ensuring
       robust and seamless operation, underscoring the importance of
       predictability in safety-critical systems.
Good:  The tick loop runs at 40 ms. Every call inside it has an upper time
       bound, which is what I-1 requires.
```

```
Bad:   It is important to note that the CRC is computed over the header
       and the payload.
Good:  The CRC covers the header and the payload. The sync word is
       excluded.
```

```
Bad:   This approach represents a significant step forward for the team.
Good:  (delete)
```

```
Bad:   Industry best practice suggests disclosing AI assistance in
       commits.
Good:  The Linux kernel policy prescribes `Assisted-by:` and forbids an
       agent from adding `Signed-off-by:`.
```

## How this is checked

Review, and only review. A reviewer who cannot find the information in a
paragraph asks for the paragraph, not for a rewrite of its wording.
There is deliberately no mechanical prose check: the rules that matter
here are judgements, and a grep that enforces the one mechanical rule
would lend the rest a false sense of coverage.

## Related

- [C++ coding standard](../coding/cpp.md) - comments, where the rule is *why* not *what*
- [Assisted development policy](../ai/policy.md) - the same discipline applied to acceptance of code
- [System invariants](../system-invariants.md) - P-4, honesty about what we know
- [Wikipedia: Signs of AI writing](https://en.wikipedia.org/wiki/Wikipedia:Signs_of_AI_writing) - the catalogue this page draws on
