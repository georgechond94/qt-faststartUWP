#include "pch.h"
#include "QtFaststart.h"

using namespace qt_faststartUWP;
using namespace Platform;

void qt_faststartUWP::QtFaststart::CreateEncodedVideoFileFromUri(Platform::String ^ fileName, Platform::String ^ destFileName)
{


	if (!String::CompareOrdinal(fileName, destFileName)) {
		throw ref new InvalidArgumentException("Input and output files need to be different");
	}

	_wfopen_s(&infile,fileName->Data(), readMode);
	if (!infile) {
		throw ref new InvalidArgumentException("Error opening file");
	}

	/* traverse through the atoms in the file to make sure that 'moov' is
	* at the end */
	while (!feof(infile)) {
		if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1) {
			break;
		}
		atom_size = BE_32(&atom_bytes[0]);
		atom_type = BE_32(&atom_bytes[4]);

		/* keep ftyp atom */
		if (atom_type == FTYP_ATOM) {
			ftyp_atom_size = atom_size;
			free(ftyp_atom);
			ftyp_atom = (unsigned char*)malloc(ftyp_atom_size);
			if (!ftyp_atom) {
				FreeResources();
				throw ref new FailureException("Could not allocate " + atom_size + " bytes for ftyp atom");
			}
			if (fseeko(infile, -ATOM_PREAMBLE_SIZE, SEEK_CUR) ||
				fread(ftyp_atom, atom_size, 1, infile) != 1 ||
				(start_offset = ftello(infile)) < 0) {
				FreeResources();
				throw ref new FailureException("Error in movie atom");
			}
		}
		else {
			int ret;
			/* 64-bit special case */
			if (atom_size == 1) {
				if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1) {
					break;
				}
				atom_size = BE_64(&atom_bytes[0]);
				ret = fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE * 2, SEEK_CUR);
			}
			else {
				ret = fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE, SEEK_CUR);
			}
			if (ret) {
				FreeResources();
				throw ref new FailureException("Error in movie atom");
			}
		}

		if ((atom_type != FREE_ATOM) &&
			(atom_type != JUNK_ATOM) &&
			(atom_type != MDAT_ATOM) &&
			(atom_type != MOOV_ATOM) &&
			(atom_type != PNOT_ATOM) &&
			(atom_type != SKIP_ATOM) &&
			(atom_type != WIDE_ATOM) &&
			(atom_type != PICT_ATOM) &&
			(atom_type != UUID_ATOM) &&
			(atom_type != FTYP_ATOM)) {
			break;
		}
		atom_offset += atom_size;

		/* The atom header is 8 (or 16 bytes), if the atom size (which
		* includes these 8 or 16 bytes) is less than that, we won't be
		* able to continue scanning sensibly after this atom, so break. */
		if (atom_size < 8)
			break;
	}

	if (atom_type != MOOV_ATOM) {
		FreeResources();
		throw ref new FailureException("Last atom in file was not a moov atom");
	}

	/* moov atom was, in fact, the last atom in the chunk; load the whole
	* moov atom */
	if (fseeko(infile, -(int)(atom_size), SEEK_END)) {
		FreeResources();
		throw ref new FailureException("Error loading moov atom");
	}
	last_offset = ftello(infile);
	if (last_offset < 0) {
		FreeResources();
		throw ref new FailureException("Error loading moov atom");
	}
	moov_atom_size = atom_size;
	moov_atom = (unsigned char*)malloc(moov_atom_size);
	if (!moov_atom) {
		FreeResources();
		throw ref new FailureException("Could not allocate " + atom_size + " bytes for moov atom");
	}
	if (fread(moov_atom, atom_size, 1, infile) != 1) {
		FreeResources();
		throw ref new FailureException("Error reading moov atom");
	}

	/* this utility does not support compressed atoms yet, so disqualify
	* files with compressed QT atoms */
	if (BE_32(&moov_atom[12]) == CMOV_ATOM) {
		FreeResources();
		throw ref new FailureException("Compressed atoms are not supported");
	}

	/* close; will be re-opened later */
	fclose(infile);
	infile = NULL;

	/* crawl through the moov chunk in search of stco or co64 atoms */
	for (i = 4; i < moov_atom_size - 4; i++) {
		atom_type = BE_32(&moov_atom[i]);
		if (atom_type == STCO_ATOM) {
			atom_size = BE_32(&moov_atom[i - 4]);
			if (i + atom_size - 4 > moov_atom_size) {
				FreeResources();
				throw ref new FailureException("Bad atom size");
			}
			offset_count = BE_32(&moov_atom[i + 8]);
			if (i + 12 + offset_count * UINT64_C(4) > moov_atom_size) {
				FreeResources();
				throw ref new FailureException("Bad atom size/element count");
			}
			for (j = 0; j < offset_count; j++) {
				current_offset = BE_32(&moov_atom[i + 12 + j * 4]);
				current_offset += moov_atom_size;
				moov_atom[i + 12 + j * 4 + 0] = (current_offset >> 24) & 0xFF;
				moov_atom[i + 12 + j * 4 + 1] = (current_offset >> 16) & 0xFF;
				moov_atom[i + 12 + j * 4 + 2] = (current_offset >> 8) & 0xFF;
				moov_atom[i + 12 + j * 4 + 3] = (current_offset >> 0) & 0xFF;
			}
			i += atom_size - 4;
		}
		else if (atom_type == CO64_ATOM) {
			atom_size = BE_32(&moov_atom[i - 4]);
			if (i + atom_size - 4 > moov_atom_size) {
				FreeResources();
				throw ref new FailureException("Bad atom size");
			}
			offset_count = BE_32(&moov_atom[i + 8]);
			if (i + 12 + offset_count * UINT64_C(8) > moov_atom_size) {
				FreeResources();
				throw ref new FailureException("Bad atom size/element count");
			}
			for (j = 0; j < offset_count; j++) {
				current_offset = BE_64(&moov_atom[i + 12 + j * 8]);
				current_offset += moov_atom_size;
				moov_atom[i + 12 + j * 8 + 0] = (current_offset >> 56) & 0xFF;
				moov_atom[i + 12 + j * 8 + 1] = (current_offset >> 48) & 0xFF;
				moov_atom[i + 12 + j * 8 + 2] = (current_offset >> 40) & 0xFF;
				moov_atom[i + 12 + j * 8 + 3] = (current_offset >> 32) & 0xFF;
				moov_atom[i + 12 + j * 8 + 4] = (current_offset >> 24) & 0xFF;
				moov_atom[i + 12 + j * 8 + 5] = (current_offset >> 16) & 0xFF;
				moov_atom[i + 12 + j * 8 + 6] = (current_offset >> 8) & 0xFF;
				moov_atom[i + 12 + j * 8 + 7] = (current_offset >> 0) & 0xFF;
			}
			i += atom_size - 4;
		}
	}

	/* re-open the input file and open the output file */
	_wfopen_s(&infile,fileName->Data(), readMode);
	if (!infile) {
		FreeResources();
		throw ref new FailureException("File not found");
	}

	if (start_offset > 0) { /* seek after ftyp atom */
		if (fseeko(infile, start_offset, SEEK_SET)) {
			FreeResources();
			throw ref new FailureException("Error opening file");
		}

		last_offset -= start_offset;
	}

	_wfopen_s(&outfile,destFileName->Data(), writeMode);
	if (!outfile) {
		FreeResources();
		throw ref new FailureException("Error creating new file");
	}

	/* dump the same ftyp atom */
	if (ftyp_atom_size > 0) {
		if (fwrite(ftyp_atom, ftyp_atom_size, 1, outfile) != 1) {
			FreeResources();
			throw ref new FailureException("Error writing ftyp atom");
		}
	}

	/* dump the new moov atom */
	printf(" writing moov atom...\n");
	if (fwrite(moov_atom, moov_atom_size, 1, outfile) != 1) {
		FreeResources();
		throw ref new FailureException("Error writing moov atom");
	}

	/* copy the remainder of the infile, from offset 0 -> last_offset - 1 */
	bytes_to_copy = MIN(COPY_BUFFER_SIZE, last_offset);
	copy_buffer = (unsigned char*)malloc(bytes_to_copy);

	if (!copy_buffer) {
		FreeResources();
		throw ref new FailureException("Could not allocate " + bytes_to_copy + " bytes");
	}
	while (last_offset) {
		bytes_to_copy = MIN(bytes_to_copy, last_offset);

		if (fread(copy_buffer, bytes_to_copy, 1, infile) != 1) {
			FreeResources();
			throw ref new FailureException("Error writing video data");
		}
		if (fwrite(copy_buffer, bytes_to_copy, 1, outfile) != 1) {
			FreeResources();
			throw ref new FailureException("Error writing video data");
		}
		last_offset -= bytes_to_copy;
	}

	FreeResources();

}

void qt_faststartUWP::QtFaststart::FreeResources()
{
	fclose(infile);
	fclose(outfile);
	free(moov_atom);
	free(ftyp_atom);
	free(copy_buffer);
}
