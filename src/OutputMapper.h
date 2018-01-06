/**
 * The output mapper builds a relation between output groups (or a collection of
 * groups, called an ubergroup) and an effect routine.
 */
#ifndef OUTPUTMAPPER_H
#define OUTPUTMAPPER_H

#include "HSIPixel.h"
#include "db/Group.h"

#include <map>
#include <set>
#include <vector>

#include "INIReader.h"

class Routine;

class DataStore;
class Framebuffer;

class OutputMapper {
	friend class EffectRunner;

	public:
		class OutputGroup {
			friend class OutputMapper;

			public:
				OutputGroup() = delete;
				OutputGroup(DbGroup *g);
				virtual ~OutputGroup();

				/**
				 * Returns an iterator into the group's framebuffer.
				 */
				HSIPixel *getDataPointer() {
					return this->buffer;
				}

				/**
				 * Returns the number of pixels in the group.
				 */
				virtual int numPixels() const {
					return this->group->numPixels();
				}

				virtual void bindBufferToRoutine(Routine *r);
				virtual void copyIntoFramebuffer(Framebuffer *fb);

			private:
				virtual void _resizeBuffer();

				bool bufferChanged = false;
				Routine *bufferBoundRoutine = nullptr;

				HSIPixel *buffer = nullptr;
				size_t bufferSz = 0;

			private:
				DbGroup *group = nullptr;

				friend bool operator==(const OutputGroup& lhs, const OutputGroup& rhs);
				friend bool operator< (const OutputGroup& lhs, const OutputGroup& rhs);
		};

		class OutputUberGroup: public OutputGroup {
			friend class OutputMapper;

			public:
				OutputUberGroup();
				OutputUberGroup(std::vector<OutputGroup *> &members);
				~OutputUberGroup();

			public:
				// overrides from OutputGroup
				virtual int numPixels() const;

				virtual void copyIntoFramebuffer(Framebuffer *fb);

			private:
				void addMember(OutputGroup *group);
				void removeMember(OutputGroup *group);
				bool containsMember(OutputGroup *group);

			private:
				std::set<OutputGroup *> groups;
		};

	public:
		OutputMapper(DataStore *s, Framebuffer *f, INIReader *reader);
		~OutputMapper();

	public:
		void addMapping(OutputGroup *g, Routine *r);
		void removeMappingForGroup(OutputGroup *g);

		inline Routine *routineForMapping(OutputGroup *g) {
			return this->outputMap[g];
		}

	private:
		void _removeMappingsInUbergroup(OutputUberGroup *ug);

	private:
		DataStore *store;
		Framebuffer *fb;
		INIReader *config;

		std::map<OutputGroup *, Routine *> outputMap;
};

#endif
