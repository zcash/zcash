*WARNING:* I, Nathan, just wrote this up as a braindump and I haven't
even seen the Sapling Spec yet! So don't treat this as anything but a
completely tentative proposal.

**Proposal for Sapling Consensus Rules:**

Have we begun to specify the high-level consensus rules for the Sapling
protocol?

Here's my attempt at requirements and strawman design to clarify this
conversation. A better home for this, if it doesn't yet exist elsewhere,
is probably in a ZIP or the Sapling Spec.

**Terminology:**

-  ``Sapling Consensus Protocol:`` The entire consensus protocol ruleset
   from the point of Sapling activation until the subsequent protocol
   activation.
-  ``Sapling Proof:`` Either a Sapling Input or Sapling Output proof.
-  ``Sprout Proof:`` A proof using the Sprout circuit construction. More
   specifically:
   ``Sprout/PGHR13 vs Sprout/Groth16tweaked-Jubjub-PowersOfTau``
-  ``Z1 Address:`` A shielded address for the Sprout circuit. (As of
   today, the only version of z-addr.)
-  ``Z2 Address:`` A shielded address for the Sapling circuit
   construction. (Tangent: The same address format may also be used in
   distinct future circuits.)
-  This ticket comment introduces these new field names:

-  ``vsaplingin`` - a vector of ``Sapling Input Proofs``.
-  ``vpub_new_sapling`` - a field associated with each
   ``Sapling Input Proof`` analogous to ``vpub_new`` for
   ``Sprout Proofs``.
-  ``vsaplingout`` - a vector of ``Sapling Output Proofs``.
-  ``vpub_old_sapling`` - a field associated with each
   ``Sapling Output Proof`` analogous to ``vpub_old`` for
   ``Sprout Proofs``.

**Sapling Protocol Consensus Requirements:**

-  **R1. Transparent Transfers Unchanged:** There are no significant
   changes to transparent transfer functionality as compared to the
   Overwinter consensus rules.
-  **R2. Z1 Receive:** Transferring funds into a Z1 address is (still)
   possible using a ``Sprout Proof``.
-  **R3. Z1 Send:** Transferring funds out of a Z1 address is (still)
   possible using a ``Sprout Proof``.
-  **R4. Z2 Receive:** Transferring funds into a Z2 address is possible
   with a ``Sapling Output Proof``.
-  **R5. Z2 Send:** Transferring funds out of a Z2 address is possible
   with a ``Sapling Input Proof``.
-  **R6. Transparent Balance Equation:** The transparent value flowing
   into the transaction must equal the transparent value flowing out:

-  the sum of ZEC coming from all transparent inputs via ``vin``, Sprout
   deshielding from the ``vpub_new`` fields within ``vjoinsplit``, and
   Sapling deshielding funds from the ``vpub_new_sapling`` fields within
   ``vsaplingin``, **MINUS**
-  the sum of ZEC coming going to transparent outputs via ``vout``,
   Sprout shielding into the ``vpub_old`` fields within ``vjoinsplit``,
   Sapling shielding into the ``vpub_new_sapling`` fields within
   ``vsaplingin``, and finally the transparent transaction fee
-  is exactly zero.

-  **R7. Sprout Balance Equation:** The total value flowing into Sprout
   proofs within a transaction must equal the total value flowing our of
   Sprout proofs within that transaction:

-  the sum of ZEC coming from shielded ``Sprout input notes`` and
   ``vpub_old`` fields for all of ``vjoinsplit``, **MINUS**
-  the sum of ZEC going to shielded ``Sprout output notes`` and
   ``vpub_new`` fields for all of ``vjoinsplit``.
-  is exactly zero

-  **R8. Sapling Balance Equation:** The total value flowing into
   Sapling proofs within a transaction must equal the total value
   flowing our of Sapling proofs within that transaction:

-  the sum of ZEC coming from shielded ``Sapling input notes`` and
   ``vpub_old_sapling`` fields for all of ``vsaplingin``, **MINUS**
-  the sum of ZEC going to shielded ``Sapling output notes`` and
   ``vpub_new_sapling`` fields for all of ``vsaplingout``
-  is exactly zero.

-  **R9. Mandatory Transparency Across Z1↔Z2 Transfers:** Only **R6**
   allows transfer of value from ``Z1`` addresses to/from ``Z2``
   addresses. There is no blinding or hiding available for transferring
   across address types.

**Notes:**

-  I'm anxious about specifying **R6**, **R7**, and **R8** correctly.
-  **R8** is deliberately only about the whole of all Sapling proofs
   within a transaction, so it allows blinded value carries between
   proofs within the Sapling ``vsaplingin/out`` vectors.
-  I'm not familiar with Sapling yet, so I'm doing a lot of guesswork
   about the outside interface.
